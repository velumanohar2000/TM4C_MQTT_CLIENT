// MQTT Library (framework only)
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
#include "mqtt.h"
#include "timer.h"

// ------------------------------------------------------------------------------
//  Globals
// ------------------------------------------------------------------------------
uint16_t INDEX = 0;
// ------------------------------------------------------------------------------
//  Structures
// ------------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

uint16_t sendConnect(uint8_t *sendCon, char *clientId)
{
    uint8_t x;
    uint16_t i;
    i = 0;

    sendCon[i++] = 0x10; // connect command

    sendCon[i++] = 0; // remaining length

    sendCon[i++] = 0; // msb
    sendCon[i++] = 4; // lsb
    sendCon[i++] = 'M';
    sendCon[i++] = 'Q';
    sendCon[i++] = 'T';
    sendCon[i++] = 'T';
    sendCon[i++] = 4;   // level
    sendCon[i++] = 0x2; // clean session flag
    sendCon[i++] = 0;   // msb
    sendCon[i++] = 120; // lsb keep alive

    uint16_t strLenClientId = strlen(clientId);
    sendCon[i++] = strLenClientId & 0xFF00;
    sendCon[i++] = strLenClientId & 0x00FF;

    for (x = 0; x < strlen(clientId); x++)
    {
        sendCon[i++] = clientId[x];
    }

    sendCon[1] = i - 2;

    return i;
}

uint16_t sendPublish(char *topic, char *data, uint8_t *sendPublish)
{
    uint8_t x;
    uint32_t i = 0;

    //    x = 14;
    //    do
    //    {
    //        encodedByte = x % 128;
    //        x = x / 128;
    //        if ( x > 0 )
    //        {
    //            encodedByte = encodedByte | 128;
    //        }
    //    }while ( x > 0 );

    uint16_t totalLength = strlen(topic) + strlen(data) + 6 + 2;

    uint16_t strLenTopic = strlen(topic);
    uint16_t strLenData = strlen(data);

    sendPublish[i++] = (3 << 4);
    sendPublish[i++] = 0;
    sendPublish[i++] = strLenTopic & 0xFF00;
    sendPublish[i++] = strLenTopic & 0x00FF;
    for (x = 0; x < strlen(topic); x++)
    {
        sendPublish[i++] = topic[x];
    }
    sendPublish[i++] = strLenData & 0xFF00;
    sendPublish[i++] = strLenData & 0x00FF;
    for (x = 0; x < strlen(data); x++)
    {
        sendPublish[i++] = data[x];
    }
    sendPublish[1] = i - 2;

    return i;
}
//--------------------------------------------------------------------------------------
uint16_t sendSub(char *topicFilter, uint8_t *sendSub, uint16_t packetId)
{
    uint8_t x;
    uint32_t i = 0;

    //    x = 14;
    //    do
    //    {
    //        encodedByte = x % 128;
    //        x = x / 128;
    //        if ( x > 0 )
    //        {
    //            encodedByte = encodedByte | 128;
    //        }
    //    }while ( x > 0 );

    uint16_t strLenTopicFilter = strlen(topicFilter);

    sendSub[i++] = (8 << 4) | 2;
    sendSub[i++] = 0;
    sendSub[i++] = packetId & 0xFF00;
    sendSub[i++] = packetId & 0x00FF;

    sendSub[i++] = strLenTopicFilter & 0xFF00;
    sendSub[i++] = strLenTopicFilter & 0x00FF;

    for (x = 0; x < strlen(topicFilter); x++)
    {
        sendSub[i++] = topicFilter[x];
        filterTopics[INDEX][x] = topicFilter[x];
    }
    filterTopics[INDEX][x] = '\0';
    packetIDArray[INDEX] = packetId;
    uint16_t packetIdCheck = packetIDArray[INDEX];
    char stringCheck[100];

    strcpy(stringCheck, filterTopics[INDEX]);

    sendSub[i++] = 0;
    sendSub[1] = i - 2;
    INDEX++;
    return i;
}

uint16_t sendUnsubFunc(char *unsubTopicFilter, uint8_t *sendUnsubscribe)
{
    uint8_t i = 0;
    uint16_t x;
    sendUnsubscribe[i++] = (10 << 4) | 2;
    sendUnsubscribe[i++] = 0;

    uint8_t packetIdIndex = getPacketIdIndex(unsubTopicFilter);
    uint16_t stringLength = strlen(unsubTopicFilter);
    if (packetIdIndex != 101)
    {
        uint16_t packetId = packetIDArray[packetIdIndex];
        packetIDArray[packetIdIndex] = -1; // set unsubscribed topic to -1, so that it is not referenced again

        sendUnsubscribe[i++] = packetId & 0xFF00; // MSB for packet ID
        sendUnsubscribe[i++] = packetId & 0x00FF; // LSB for packet ID

        sendUnsubscribe[i++] = stringLength & 0xFF00;
        sendUnsubscribe[i++] = stringLength & 0x00FF;

        for (x = 0; x < strlen(unsubTopicFilter); x++)
        {
            sendUnsubscribe[i++] = unsubTopicFilter[x];
        }
        sendUnsubscribe[1] = i - 2;
        return i;
    }
    else
    {
        sendUnsubscribe[i++] = 0; // MSB for packet ID
        sendUnsubscribe[i++] = 0; // LSB for packet ID
        return 65535;
    }

    
}

uint8_t getPacketIdIndex(char *topicFilter)
{
    uint8_t x = 0;
    for (x = 0; x < INDEX; x++)
    {
        if (strcmp(topicFilter, filterTopics[x]) == 0)
        {
            if (packetIDArray[x] >= 0)
            {
                return x;
            }
        }
    }
    return 101;
}


uint16_t sendDisconnect(uint8_t *sendDisconnect)
{
    uint16_t i = 0;

    sendDisconnect[i++] = (14 << 4); // MQTT Disconnect command
    sendDisconnect[i++] = 0; // remaining length
    return i;
}


