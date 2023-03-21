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
#include <inttypes.h>


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
bool ok;
uint8_t sendMQTTData [1522];
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

   //  copy data
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
    uint32_t size;
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
        //putsUart0("\nSTATE: SYN SENT\n");
        break;
        // TCP state is established once 3 way handshake complete,
        // Will enter MQTT state machine
    case TCP_ESTABLISHED:
        // MQTT state machine
        switch (MQTT_STATE)
        {
        // Will try to connect to mqtt broker
        case MQTT_CONNECT:
            //putsUart0("\nSTATE:\n\tMQTT: SENDING CON\n");
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
            // ACKING, a CONNACK, SUBACK, or UNSUBACK
        case MQTT_PACKETACK_ACK:
            size = mqttFixed->remainingLengthLSB + 2;
            s->acknowledgementNumber += size;
            saveSeqAck(s);
            FLAGS |= ACK;
            MQTT_STATE = MQTT_WAIT;
            sendTcpMessage(ether, *s, NULL, 0);
            break;
        case MQTT_PUBACK_ACK:
            //uint32_t remainingLength;
            ok = decodePub(ether,s);
            if (ok)
            {
                FLAGS |= ACK;
                sendTcpMessage(ether, *s, NULL, 0);
            }
            else
            {
                putsUart0("Unable to decode\n");
            }

            MQTT_STATE = MQTT_WAIT;
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
    //sendTcpMessage(ether, *s, sendPub, totalLength);
    memcpy(sendMQTTData, (uint8_t*)sendPub, totalLength);
    sendTcpMessage(ether, *s, sendMQTTData, totalLength);
}
// function for connecting to mqtt
void sendMqttConnect(etherHeader *ether, socket *s)
{
    uint8_t *sendCon = (uint8_t *)getTcpData(ether);
    uint16_t totalLength = sendConnect(sendCon, "velu");
    FLAGS = (PSH | ACK);
    s->acknowledgementNumber = currentAck;
    s->sequenceNumber = currentSeq;
    //sendTcpMessage(ether, *s, sendCon, totalLength);
    memcpy(sendMQTTData, (uint8_t*)sendCon, totalLength);
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
    //sendTcpMessage(ether, *s, sendSubscribe, totalLength);
    memcpy(sendMQTTData, (uint8_t*)sendSubscribe, totalLength);
    sendTcpMessage(ether, *s, sendMQTTData, totalLength);
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
        //sendTcpMessage(ether, *s, sendUnsub, totalLength);
        memcpy(sendMQTTData, (uint8_t*)sendUnsub, totalLength);
        sendTcpMessage(ether, *s, sendMQTTData, totalLength);
    }
    else
    {
        // if could not find topic, then send an error message
        putsUart0("Unable to find topic name, please try again.\n");
        MQTT_STATE = MQTT_WAIT;
    }
}
// function for disconnecting from mqtt
void sendMqttDisconnect(etherHeader *ether, socket *s)
{
    uint8_t *sendDiscon = (uint8_t *)getTcpData(ether);
    uint16_t totalLength = sendDisconnect(sendDiscon);
    FLAGS = (PSH |  ACK);
    s->acknowledgementNumber = currentAck;
    s->sequenceNumber = currentSeq;
    //TCP_STATE = TCP_FIN_WAIT_1;
    memcpy(sendMQTTData, (uint8_t*)sendDiscon, totalLength);
    sendTcpMessage(ether, *s, sendMQTTData, totalLength);
}
// function for printing recieved publish message to putty,


bool decodePub(etherHeader *ether, socket *s)
{
    uint16_t i;
    uint16_t x;
//    char printTopic[256];
//    char printData[2048];
    char *decodedPubArray = (char *)getTcpData(ether);

    uint16_t multiplier = 1;
    uint16_t value = 0;
    char* ptr = decodedPubArray+1;
    char encodedByte;
    do {
        encodedByte = *ptr++;   //
        value += (encodedByte & 127) * multiplier;
        multiplier *= 128;    
        if (multiplier > 128*128*128) 
        {
            return 0; // remaining length too long
        }
    } while ((encodedByte & 128) != 0);

    uint16_t currentIndex =  ptr - decodedPubArray;   // returns how many bytes that have been read
    uint16_t bytesRead = currentIndex;
    uint16_t topicLength = decodedPubArray[currentIndex] | decodedPubArray[currentIndex+1];
    currentIndex += 1;
    putsUart0("TOPIC: ");
    //    for (i = 0; i < topicLength+1; i++)
    //    {
    //        printTopic[i] = decodedPubArray[currentIndex++];
    //    }
    //    putsUart0(printTopic);
    for (i = 0; i < topicLength+1; i++)
    {
        putcUart0(((char)decodedPubArray[currentIndex++]));
    }
    //putsUart0(((char*)decodedPubArray[currentIndex++]));
    //printTopic[i] = '\0';
    // putsUart0(printTopic);
    putsUart0("\n");
    putsUart0("DATA: ");
    uint16_t dataLength = value - topicLength - 2; // publishPayLoad->data[i] | publishPayLoad->data[i + 1];
    for (x = 0; x < dataLength; x++)
    {
        putcUart0(((char)decodedPubArray[currentIndex++]));
    }
    //printData[x] = '\0';
   // putsUart0(printData);
    putsUart0("\n");


    s->acknowledgementNumber += value+bytesRead;
    saveSeqAck(s);


    return 1;


}


void getTcpMqttStatus()
{
    uint8_t i;
    char str[10];
    uint8_t ip[4];
    putcUart0('\n');
    getIpAddress(ip);
    putsUart0("IP:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putsUart0(" (static)");
    putcUart0('\n');
    getIpMqttBrokerAddress(ip);
    putsUart0("MQTT:  ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');

    bool mqttStatus = false;
    putsUart0("TCP STATE: ");
    switch (TCP_STATE)
    {
    case TCP_CLOSED:
        putsUart0("CLOSED\n");
        break;
    case TCP_SYN_SENT:
        putsUart0("SYN-SENT\n");
        break;
    case TCP_ESTABLISHED:
        mqttStatus = true;
        putsUart0("Established\n");
        switch (MQTT_STATE)
        {
        case MQTT_PUBLISH:
        case MQTT_SUBSCRIBE:
        case MQTT_DISCONNECT:
        case MQTT_UNSUBSCRIBE:
        case MQTT_PACKETACK_ACK:
        case MQTT_PUBACK_ACK:
        case MQTT_WAIT:
        case MQTT_CONNECT:
            putsUart0("MQTT STATE: CONNECTED\n");
            break;
        default:
            putsUart0("MQTT STATE: NOT CONNECTED\n");
            break;
        }
        break;
        case TCP_FIN_WAIT_1:
            putsUart0("FIN WAIT 1\n");
            break;
        case TCP_FIN_WAIT_2:
            putsUart0("FIN WAIT 2\n");
            break;
        case TCP_LAST_ACK:
            putsUart0("LAST ACK\n");
            break;
        case TCP_CLOSE_WAIT:
            putsUart0("CLOSED\n");
            break;
    }
    if (!mqttStatus)
        putsUart0("MQTT STATE: NOT CONNECTED\n");

}








// void printToUart(etherHeader *ether, socket *s, uint16_t index, uint32_t remainingLength)
// {

//     MQTT_FIXED *mqttFixed = (MQTT_FIXED *)getTcpData(ether);
//     PUBLISH_PAYLOAD *publishPayLoad = (PUBLISH_PAYLOAD *)mqttFixed->data;
//     uint16_t i, x;
//     //uint16_t topicLength =
//     for (i = 0; i < ntohs(publishPayLoad->lengthTopic); i++)
//     {
//         printTopic[i] = publishPayLoad->data[index++];
//     }
//     printTopic[i] = '\0';
//     putsUart0("TOPIC: ");
//     putsUart0(printTopic);
//     putsUart0("\n");

//     uint16_t dataLength = mqttFixed->remainingLengthLSB - ntohs(publishPayLoad->lengthTopic) - 2; // publishPayLoad->data[i] | publishPayLoad->data[i + 1];
//     for (x = 0; x < dataLength; x++)
//     {
//         printData[x] = publishPayLoad->data[i + x];
//     }
//     printData[x] = '\0';
//     putsUart0("DATA: ");
//     putsUart0(printData);
//     putsUart0("\n");



//     return;
// }
