/*
 * IoT_Project1
 *
 *  Created on: March 19, 2023
 *      Author: Velu Manohar
 */

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: -
// Target uC:       -
// System Clock:    -

// Hardware configuration:
// -

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "tcp.h"
#include "timer.h"
#include "mqtt.h"
#include "uart0.h"

// define MAX_PACKET_SIZE 1522

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------
uint16_t FLAGS;
uint32_t currentSeq;
uint32_t currentAck;
bool done = false;
bool ACK_NOT_SENT_FLAG = 1;
bool MQTT_CONACK_ACK_NOT_SENT = 1;
bool MQTT_PUB_NOT_SENT = 1;
// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Determines whether packet is TCP packet
// Must be an IP packet
bool isTcp(etherHeader *ether)
{
    ipHeader *ip = (ipHeader *)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader *)((uint8_t *)ip + ipHeaderLength);
    uint32_t sum = 0;
    bool ok;
    uint16_t tmp16;
    ok = (ip->protocol == PROTOCOL_TCP);
    if (ok)
    {
        // 32-bit sum over pseudo-header
        sumIpWords(ip->sourceIp, 8, &sum);
        tmp16 = ip->protocol;
        sum += (tmp16 & 0xff) << 8;
        tmp16 = htons(ntohs(ip->length) - (ip->size * 4));
        sumIpWords(&tmp16, 2, &sum);
        // add tcp header and data
        sumIpWords(tcp, ntohs(ip->length) - (ip->size * 4), &sum);
        ok = (getIpChecksum(sum) == 0);
    }
    return ok;
}

// getting tcp data from ether
uint8_t *getTcpData(etherHeader *ether)
{
    ipHeader *ip = (ipHeader *)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader *)((uint8_t *)ip + ipHeaderLength);
    return tcp->data;
}

// getting sequence and acknowledgement numbers from received packet
void getSeqACK(etherHeader *ether, socket *s)
{
    ipHeader *ip = (ipHeader *)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader *)((uint8_t *)ip + ipHeaderLength);
    s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
    s->acknowledgementNumber = ntohl(tcp->sequenceNumber);
}

// sending tcp message on ether
void sendTcpMessage(etherHeader *ether, socket s, uint8_t data[], uint16_t dataSize)
{
    uint8_t i;
    uint32_t sum;
    uint16_t tmp16;
    uint16_t tcpLength;
    uint8_t *copyData;
    uint8_t localHwAddress[6];
    uint8_t localIpAddress[4];

    // Ether frame
    getEtherMacAddress(localHwAddress);
    getIpAddress(localIpAddress);
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        ether->destAddress[i] = s.remoteHwAddress[i];
        ether->sourceAddress[i] = localHwAddress[i];
    }
    ether->frameType = htons(TYPE_IP);

    // Initializing IP header
    ipHeader *ip = (ipHeader *)ether->data;
    ip->rev = 0x4;  // for IPV4
    ip->size = 0x5; // often 5 (slides)
    ip->typeOfService = 0;
    ip->id = 0;
    ip->flagsAndOffset = 0;
    ip->ttl = 128; // time to live
    ip->protocol = PROTOCOL_TCP;
    ip->headerChecksum = 0;

    uint8_t ipHeaderLength = ip->size * 4;

    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        ip->destIp[i] = s.remoteIpAddress[i];
        ip->sourceIp[i] = localIpAddress[i];
    }

    // TCP header
    tcpHeader *tcp = (tcpHeader *)((uint8_t *)ip + (ip->size * 4));
    tcp->sourcePort = htons(s.localPort);
    tcp->destPort = htons(s.remotePort);
    tcp->windowSize = htons(1522);

    // adjust lengths
    tcpLength = sizeof(tcpHeader) + dataSize;
    ip->length = htons(ipHeaderLength + tcpLength);

    // 32-bit sum over ip header
    calcIpChecksum(ip);

    // where TCP flags are located
    uint16_t offset = 0x0000;
    offset = 5 << OFS_SHIFT;
    offset |= FLAGS;
    tcp->offsetFields = htons(offset);

    // setting sequence and acknowledgement numbers
    tcp->sequenceNumber = htonl(s.sequenceNumber);
    tcp->acknowledgementNumber = htonl(s.acknowledgementNumber);

    // copy data
    copyData = tcp->data;
    for (i = 0; i < dataSize; i++)
        copyData[i] = data[i];

    // 32-bit checksum over pseudo-header
    sum = 0;
    sumIpWords(ip->sourceIp, 8, &sum);
    tmp16 = ip->protocol;
    sum += (tmp16 & 0xff) << 8;
    uint16_t tcpLengthHtons = htons(tcpLength);
    sumIpWords(&tcpLengthHtons, 2, &sum);
    tcp->checksum = 0;
    sumIpWords(tcp, tcpLength, &sum);
    tcp->checksum = getIpChecksum(sum);

    // send packet with size = ether + tcp hdr + ip header + tcp_size
    putEtherPacket(ether, sizeof(etherHeader) + ipHeaderLength + tcpLength);
}

// TCP state machine, that set flags and calls function depending of which state it is in
void stateMachine(etherHeader *ether, socket *s)
{
    MQTT_FIXED *mqttFixed = (MQTT_FIXED *)getTcpData(ether);
    uint8_t size;
    FLAGS = 0x0000;

    switch (TCP_STATE)
    {
    // TCP_CLOSED state: check if there is a hw address,
    // if there is then it will try connecting mqtt broker if not it will send an arp request
    case TCP_CLOSED:
        if (s->remoteHwAddress == NULL)
        {
            sendArpFlag = 1;
        }
        else
        {
            FLAGS = SYN;
            s->sequenceNumber = random32();
            s->acknowledgementNumber = 0;
            sendTcpMessage(ether, *s, NULL, 0);
        }
        break;
    // SYN has been sent, and will enter this state once SYN and ACK received
    // from broker, in this state it will ACK complete 3 way handshake
    case TCP_SYN_SENT:
        s->acknowledgementNumber += 1;
        saveSeqAck(s);
        FLAGS = ACK;
        sendTcpMessage(ether, *s, NULL, 0);
        TCP_STATE = TCP_ESTABLISHED;
        putsUart0("\nSTATE: SYN SENT\n");
        break;
    // TCP state is established once 3 way handshake complete,
    // Will enter MQTT state machine
    case TCP_ESTABLISHED:
        // MQTT state machine
        switch (MQTT_STATE)
        {
        // Will try to connect to mqtt broker
        case MQTT_CONNECT:
            putsUart0("\nSTATE:\n\tMQTT: SENDING CON\n");
            TCP_STATE = TCP_ESTABLISHED;
            sendMqttConnect(ether, s);
            break;
        // publishing message
        case MQTT_PUBLISH:
            sendMqttPub(ether, s);
            break;
        // subscibing to topic
        case MQTT_SUBSCRIBE:
            sendMqttSub(ether, s);
            break;
        // disocnonnecting from mqtt broker
        case MQTT_DISCONNECT:
            sendMqttDisconnect(ether, s);
            break;
        // unsubscribing from topic
        case MQTT_UNSUBSCRIBE:
            sendMqttUnsub(ether, s);
            break;
        // ACKING, a CONNACK, SUBACK, PUBACK, or UNSUBACK
        case MQTT_PACKETACK_ACK:
            size = mqttFixed->remainingLengthLSB + 2;
            s->acknowledgementNumber += size;
            saveSeqAck(s);
            FLAGS |= ACK;
            MQTT_STATE = MQTT_WAIT;
            sendTcpMessage(ether, *s, NULL, 0);
            if (isPUB)
            {
                printToUart(ether, s);
                isPUB = false;
            }
            break;
        case MQTT_WAIT:
            break;
        }
        
        break;
    // initializing disconnect
    case TCP_FIN_WAIT_1:
        s->acknowledgementNumber = currentAck;
        s->sequenceNumber = currentSeq;
        FLAGS |= (FIN | ACK);
        sendTcpMessage(ether, *s, NULL, 0);
        break;
    // responding to disconnect
    case TCP_FIN_WAIT_2:
        s->acknowledgementNumber += 1;
        FLAGS |= (FIN | ACK);
        sendTcpMessage(ether, *s, NULL, 0);
        break;
    // last ack, then connection is closed
    case TCP_LAST_ACK:
        FLAGS |= ACK;
        s->acknowledgementNumber += 1;
        sendTcpMessage(ether, *s, NULL, 0);
        TCP_STATE = TCP_CLOSE_WAIT;
        break;
    case TCP_CLOSE_WAIT:
        break;
    }
}

// Saving sequence number and acknowledgement number in global variables
void saveSeqAck(socket *s)
{
    currentSeq = s->sequenceNumber;
    currentAck = s->acknowledgementNumber;
}
// function for publishing message
void sendMqttPub(etherHeader *ether, socket *s)
{
    uint8_t *sendPub = getTcpData(ether);
    uint16_t totalLength = sendPublish(publishTopic, publishData, sendPub);
    FLAGS |= (PSH | ACK);
    s->acknowledgementNumber = currentAck;
    s->sequenceNumber = currentSeq;
    sendTcpMessage(ether, *s, sendPub, totalLength);
}
// function for connecting to mqtt
void sendMqttConnect(etherHeader *ether, socket *s)
{
    uint8_t *sendCon = (uint8_t *)getTcpData(ether);
    uint16_t totalLength = sendConnect(sendCon, "velu");
    FLAGS = (PSH | ACK);
    s->acknowledgementNumber = currentAck;
    s->sequenceNumber = currentSeq;
    sendTcpMessage(ether, *s, sendCon, totalLength);
}
// connection for subscribing
void sendMqttSub(etherHeader *ether, socket *s)
{
    uint8_t *sendSubscribe = (uint8_t *)getTcpData(ether);
    uint16_t packetId = random32() & 0xFFFF;
    uint16_t totalLength = sendSub(subTopicFilter, sendSubscribe, packetId);
    FLAGS = (PSH | ACK);
    s->acknowledgementNumber = currentAck;
    s->sequenceNumber = currentSeq;
    sendTcpMessage(ether, *s, sendSubscribe, totalLength);
}
// connection for unsubscribing from topic
void sendMqttUnsub(etherHeader *ether, socket *s)
{
    uint8_t *sendUnsub = (uint8_t *)getTcpData(ether);
    uint16_t totalLength = sendUnsubFunc(unsubTopicFilter, sendUnsub);
    if (totalLength != 65535)
    {
        FLAGS = (PSH | ACK);
        s->acknowledgementNumber = currentAck;
        s->sequenceNumber = currentSeq;
        sendTcpMessage(ether, *s, sendUnsub, totalLength);
    }
    else
    {
        // if could not find topic, then send an error message
        putsUart0("Unable to find topic name, please try again\n");
        MQTT_STATE = MQTT_WAIT;
    }
}
// function for disconnecting from mqtt
void sendMqttDisconnect(etherHeader *ether, socket *s)
{
    uint8_t *sendDiscon = (uint8_t *)getTcpData(ether);
    uint16_t totalLength = sendDisconnect(sendDiscon);
    FLAGS = (FIN | ACK);
    s->acknowledgementNumber = currentAck;
    s->sequenceNumber = currentSeq;
    TCP_STATE = TCP_FIN_WAIT_1;
    sendTcpMessage(ether, *s, sendDiscon, totalLength);
}
// function for printing recieved publish message to putty,
void printToUart(etherHeader *ether, socket *s)
{
    char printTopic[256];
    char printData[256];
    MQTT_FIXED *mqttFixed = (MQTT_FIXED *)getTcpData(ether);
    PUBLISH_PAYLOAD *publishPayLoad = (PUBLISH_PAYLOAD *)mqttFixed->data;
    uint16_t i, x;
    for (i = 0; i < ntohs(publishPayLoad->lengthTopic); i++)
    {
        printTopic[i] = publishPayLoad->data[i];
    }
    printTopic[i] = '\0';
    putsUart0("TOPIC: ");
    putsUart0(printTopic);
    putsUart0("\n");

    uint16_t dataLength = publishPayLoad->data[i] | publishPayLoad->data[i + 1];
    for (x = 0; x < dataLength; x++)
    {
        printData[x] = publishPayLoad->data[i + 2 + x];
    }
    printData[x] = '\0';
    putsUart0("DATA: ");
    putsUart0(printData);
    putsUart0("\n");
    return;
}

// case TCP_LAST_FIN_ACK:
//         FLAGS |= (FIN | ACK);
//         s->acknowledgementNumber += 1;
//         sendTcpMessage(ether, *s, NULL, 0);
//         TCP_STATE = 18;
//         break;

/*
else if (MQTT_STATE == MQTT_KEEP_ALIVE)
        {
            if (mqttFixed->mqttControlType == (3 << 4))
            {
                char data[80];
                char putsString[100];
                uint8_t i = 0;
                uint8_t x = 0;
                for (i = 2; i < mqttFixed->remainingLengthLSB; i++)
                {
                    data[x++] = mqttFixed->data[i];
                }

                snprintf(putsString, strlen(data), "%s\n", data);
                putsUart0(putsString);
            }
            FLAGS |= ACK;
            saveSeqAck(s);
            sendTcpMessage(ether, *s, NULL, 0);
        }
*/

// if (MQTT_STATE == MQTT_CONNECT)
// {
//     putsUart0("\nSTATE:\n\tMQTT: SENDING CON\n");
//     TCP_STATE = TCP_ESTABLISHED;
//     sendMqttConnect(ether, s);
// }
// else if (MQTT_STATE == MQTT_CONACK_ACK)
// {
//     putsUart0("\nSTATE:\n\tMQTT: SENDING CONACK_ACK\n");
//     size = mqttFixed->remainingLengthLSB + 2;
//     s->acknowledgementNumber += size;
//     saveSeqAck(s);
//     FLAGS = ACK;
//     MQTT_CONACK_ACK_NOT_SENT = 0;
//     sendTcpMessage(ether, *s, NULL, 0);
// }
// else if (MQTT_STATE == MQTT_PUBLISH)
// {

//     sendMqttPub(ether, s);
// }

// else if (MQTT_STATE == MQTT_PUBACK_ACK)
// {
//     size = mqttFixed->remainingLengthLSB + 2;
//     s->acknowledgementNumber += size;
//     saveSeqAck(s);
//     FLAGS |= ACK;
//     MQTT_STATE = MQTT_WAIT;
//     sendTcpMessage(ether, *s, NULL, 0);
//     printToUart(ether, s);
// }
// else if (MQTT_STATE == MQTT_SUBSCRIBE)
// {
//     sendMqttSub(ether, s);
// }
// else if (MQTT_STATE == MQTT_SUBACK_ACK)
// {
//     size = mqttFixed->remainingLengthLSB + 2;
//     s->acknowledgementNumber += size;
//     saveSeqAck(s);
//     FLAGS |= ACK;
//     MQTT_STATE = MQTT_WAIT;
//     sendTcpMessage(ether, *s, NULL, 0);
// }
// else if (MQTT_STATE == MQTT_DISCONNECT)
// {
//     sendMqttDisconnect(ether, s);
// }

// else if (MQTT_STATE == MQTT_UNSUBSCRIBE) // && !MQTT_CONACK_ACK_NOT_SENT)
// {
//     sendMqttUnsub(ether, s);
// }
// else if (MQTT_STATE == MQTT_UNSUBACK_ACK)
// {
//     size = mqttFixed->remainingLengthLSB + 2;
//     s->acknowledgementNumber += size;
//     saveSeqAck(s);
//     FLAGS |= ACK;
//     MQTT_STATE = MQTT_WAIT;
//     sendTcpMessage(ether, *s, NULL, 0);
// }

// else if (MQTT_STATE == MQTT_WAIT)
// {
//     // do nothing
// }
