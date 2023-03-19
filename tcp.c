// TCP Library (framework only)
// Jason Losh

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

uint8_t *getTcpData(etherHeader *ether)
{
    ipHeader *ip = (ipHeader *)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader *)((uint8_t *)ip + ipHeaderLength);
    return tcp->data;
}

void getSeqACK(etherHeader *ether, socket *s)
{
    ipHeader *ip = (ipHeader *)ether->data;
    uint8_t ipHeaderLength = ip->size * 4;
    tcpHeader *tcp = (tcpHeader *)((uint8_t *)ip + ipHeaderLength);
    s->sequenceNumber = ntohl(tcp->acknowledgementNumber);
    s->acknowledgementNumber = ntohl(tcp->sequenceNumber);
}

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

    // IP header
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
    uint16_t offset = 0x0000;
    tcpHeader *tcp = (tcpHeader *)((uint8_t *)ip + (ip->size * 4));
    tcp->sourcePort = htons(s.localPort);
    tcp->destPort = htons(s.remotePort);
    tcp->windowSize = htons(1522);

    // adjust lengths
    tcpLength = sizeof(tcpHeader) + dataSize;
    ip->length = htons(ipHeaderLength + tcpLength);

    // 32-bit sum over ip header
    calcIpChecksum(ip);
    offset = 5 << OFS_SHIFT; //((tcpLength) / 4 << OFS_SHIFT);

    offset |= FLAGS;
    tcp->offsetFields = htons(offset);
    tcp->sequenceNumber = htonl(s.sequenceNumber);
    tcp->acknowledgementNumber = htonl(s.acknowledgementNumber);

    // copy data
    copyData = tcp->data;
    for (i = 0; i < dataSize; i++)
        copyData[i] = data[i];

    // 32-bit sum over pseudo-header
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

void stateMachine(etherHeader *ether, socket *s)
{
    //    uint8_t recievedOffsetField = 0;
    //    ipHeader *ip;
    //    uint8_t ipHeaderLength;
    //    tcpHeader *tcp;
    MQTT_FIXED *mqttFixed = (MQTT_FIXED *)getTcpData(ether);

    uint8_t size;

    FLAGS = 0x0000;

    switch (TCP_STATE)
    {
    case TCP_CLOSED:
        ACK_NOT_SENT_FLAG = 1;
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
    case TCP_SYN_SENT:
        //        if (ACK_NOT_SENT_FLAG)
        //        {
        s->acknowledgementNumber += 1;
        saveSeqAck(s);
        FLAGS = ACK;
        sendTcpMessage(ether, *s, NULL, 0);
        TCP_STATE = TCP_ESTABLISHED;
        putsUart0("\nSTATE: SYN SENT\n");
        ACK_NOT_SENT_FLAG = 0;
        // }
        break;

    case TCP_ESTABLISHED:
        if (MQTT_STATE == MQTT_CONNECT)
        {
            // getSeqACK(ether ,s);
            putsUart0("\nSTATE:\n\tMQTT: SENDING CON\n");
            TCP_STATE = TCP_ESTABLISHED;
            sendMqttConnect(ether, s);
            MQTT_CONACK_ACK_NOT_SENT = 1;
        }
        else if (MQTT_STATE == MQTT_CONACK_ACK)
        {
            if (MQTT_CONACK_ACK_NOT_SENT)
            {
                putsUart0("\nSTATE:\n\tMQTT: SENDING CONACK_ACK\n");
                size = mqttFixed->remainingLengthLSB + 2;
                s->acknowledgementNumber += size;
                saveSeqAck(s);
                FLAGS = ACK;
                MQTT_CONACK_ACK_NOT_SENT = 0;
                sendTcpMessage(ether, *s, NULL, 0);
            }
        }
        else if (MQTT_STATE == MQTT_PUBLISH)
        {

            sendMqttPub(ether, s);
        }

        else if (MQTT_STATE == MQTT_PUBACK_ACK)
        {
            size = mqttFixed->remainingLengthLSB + 2;
            s->acknowledgementNumber += size;
            saveSeqAck(s);
            FLAGS |= ACK;
            MQTT_STATE = MQTT_WAIT;
            sendTcpMessage(ether, *s, NULL, 0);
            printToUart(ether, s);
        }
        else if (MQTT_STATE == MQTT_SUBSCRIBE)
        {
            sendMqttSub(ether, s);
        }
        else if (MQTT_STATE == MQTT_SUBACK_ACK)
        {
            size = mqttFixed->remainingLengthLSB + 2;
            s->acknowledgementNumber += size;
            saveSeqAck(s);
            FLAGS |= ACK;
            MQTT_STATE = MQTT_WAIT;
            sendTcpMessage(ether, *s, NULL, 0);
        }
        else if (MQTT_STATE == MQTT_DISCONNECT)
        {
            sendMqttDisconnect(ether, s);
        }
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
        else if (MQTT_STATE == MQTT_UNSUBSCRIBE) // && !MQTT_CONACK_ACK_NOT_SENT)
        {
            sendMqttUnsub(ether, s);
        }
        else if (MQTT_STATE == MQTT_UNSUBACK_ACK)
        {
            size = mqttFixed->remainingLengthLSB + 2;
            s->acknowledgementNumber += size;
            saveSeqAck(s);
            FLAGS |= ACK;
            MQTT_STATE = MQTT_WAIT;
            sendTcpMessage(ether, *s, NULL, 0);
        }

        else if (MQTT_STATE == MQTT_WAIT)
        {
            // do nothing
        }
        break;

    case TCP_FIN_WAIT_2:
        s->acknowledgementNumber += 1;
        FLAGS |= ACK;
        sendTcpMessage(ether, *s, NULL, 0);
        TCP_STATE = 17;
        break;
    case TCP_CLOSE_WAIT:
        s->acknowledgementNumber += 1;
        FLAGS |= (FIN | ACK);
        sendTcpMessage(ether, *s, NULL, 0);
        TCP_STATE = TCP_LAST_ACK;
        break;

    case TCP_LAST_FIN_ACK:
        FLAGS |= (FIN | ACK);
        s->acknowledgementNumber += 1;
        sendTcpMessage(ether, *s, NULL, 0);
        TCP_STATE = 18;
        break;
    case TCP_LAST_ACK:
        FLAGS |= ACK;
        s->acknowledgementNumber += 1;
        sendTcpMessage(ether, *s, NULL, 0);
        TCP_STATE = 19;
        break;

    case TCP_FIN_WAIT_1:

        s->acknowledgementNumber = currentAck;
        s->sequenceNumber = currentSeq;
        FLAGS |= (FIN | ACK);
        sendTcpMessage(ether, *s, NULL, 0);
        break;
    }
}

void saveSeqAck(socket *s)
{
    currentSeq = s->sequenceNumber;
    currentAck = s->acknowledgementNumber;
}

void sendMqttPub(etherHeader *ether, socket *s)
{

    uint8_t *sendPub = getTcpData(ether);
    uint16_t totalLength = sendPublish(publishTopic, publishData, sendPub);
    FLAGS |= (PSH | ACK);
    s->acknowledgementNumber = currentAck;
    s->sequenceNumber = currentSeq;
    sendTcpMessage(ether, *s, sendPub, totalLength);
}

void sendMqttConnect(etherHeader *ether, socket *s)
{
    uint8_t *sendCon = (uint8_t *)getTcpData(ether);
    uint16_t totalLength = sendConnect(sendCon, "velu");
    FLAGS = (PSH | ACK);
    s->acknowledgementNumber = currentAck;
    s->sequenceNumber = currentSeq;
    sendTcpMessage(ether, *s, sendCon, totalLength);
}

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
        putsUart0("Unable to find topic name, please try again\n");
        MQTT_STATE = MQTT_WAIT;
    }
}

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

void printToUart(etherHeader *ether, socket *s)
{
    char printTopic[256];
    char printData[256];
    MQTT_FIXED *mqttFixed = (MQTT_FIXED *)getTcpData(ether);
    PUBLISH_PAYLOAD *publishPayLoad = (PUBLISH_PAYLOAD *)mqttFixed->data;
    uint16_t i, x;
    // strcpy(printTopic, publishPayLoad->data);
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
