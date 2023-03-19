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

    sendCon[i++] = MQTT_CTL_CONNECT; // connect command

    sendCon[i++] = 0; // remaining length

    sendCon[i++] = 0; // msb protocol length
    sendCon[i++] = 4; // lsb protocol length
    sendCon[i++] = 'M';
    sendCon[i++] = 'Q';
    sendCon[i++] = 'T';
    sendCon[i++] = 'T';
    sendCon[i++] = 4;   // level
    sendCon[i++] = 0x2; // clean session flag
    sendCon[i++] = 0;   // msb keep alive
    sendCon[i++] = 120; // lsb keep alive

    uint16_t strLenClientId = strlen(clientId);

    sendCon[i++] = strLenClientId & 0xFF00; // msb client id length
    sendCon[i++] = strLenClientId & 0x00FF; // lsb client id length

    for (x = 0; x < strlen(clientId); x++) // store client id
    {
        sendCon[i++] = clientId[x];
    }

    sendCon[1] = i - 2; // calculating remaining length for 1 byte

    return i; // returning total length
}

uint16_t sendPublish(char *topic, char *data, uint8_t *sendPublish)
{
    uint8_t x;
    uint32_t i = 0;

    uint16_t totalLength = strlen(topic) + strlen(data) + 6 + 2;

    uint16_t strLenTopic = strlen(topic);
    uint16_t strLenData = strlen(data);

    sendPublish[i++] = MQTT_CTL_PUBLISH;         // publish command
    sendPublish[i++] = 0;                    // place holder for remaining length
    sendPublish[i++] = strLenTopic & 0xFF00; // msb topic length
    sendPublish[i++] = strLenTopic & 0x00FF; // lsb topic length
    for (x = 0; x < strlen(topic); x++)      // store topic
    {
        sendPublish[i++] = topic[x];
    }
    sendPublish[i++] = strLenData & 0xFF00; // msb data length
    sendPublish[i++] = strLenData & 0x00FF; // lsb data length
    for (x = 0; x < strlen(data); x++)      // store data
    {
        sendPublish[i++] = data[x];
    }
    sendPublish[1] = i - 2; // calculating remaining length for 1 byte

    return i; // return total length
}
//--------------------------------------------------------------------------------------
uint16_t sendSub(char *topicFilter, uint8_t *sendSub, uint16_t packetId)
{
    uint8_t x;
    uint32_t i = 0;

    uint16_t strLenTopicFilter = strlen(topicFilter);

    sendSub[i++] = MQTT_CTL_SUBSCRIBE;    // subscribe command
    sendSub[i++] = 0;                 // placeholder for remaining length
    sendSub[i++] = packetId & 0xFF00; // lsb packet id
    sendSub[i++] = packetId & 0x00FF; // msb packet id

    sendSub[i++] = strLenTopicFilter & 0xFF00; // msb topic filter length
    sendSub[i++] = strLenTopicFilter & 0x00FF; // lsb topic filter length

    for (x = 0; x < strlen(topicFilter); x++)
    {
        sendSub[i++] = topicFilter[x];           // store topic filter
        filterTopics[INDEX][x] = topicFilter[x]; // store in local array
    }
    filterTopics[INDEX][x] = '\0';   // terminate string
    packetIDArray[INDEX] = packetId; // store packet ID
    sendSub[i++] = 0;                // qos level 0
    sendSub[1] = i - 2;              // calculating remaining length for 1 byte
    INDEX++;                         // increment local array index
    return i;                        // return total length
}

uint16_t sendUnsubFunc(char *unsubTopicFilter, uint8_t *sendUnsubscribe)
{
    uint8_t i = 0;
    uint16_t x;
    sendUnsubscribe[i++] = MQTT_CTL_UNSUB; // unsubscribe command
    sendUnsubscribe[i++] = 0;   // placeholder for remaining length

    uint8_t packetIdIndex = getPacketIdIndex(unsubTopicFilter); // get packet ID index
    uint16_t stringLength = strlen(unsubTopicFilter);
    if (packetIdIndex != 101)
    {
        uint16_t packetId = packetIDArray[packetIdIndex];
        packetIDArray[packetIdIndex] = -1; // set unsubscribed topic to -1, so that it is not referenced again

        sendUnsubscribe[i++] = packetId & 0xFF00; // MSB for packet ID
        sendUnsubscribe[i++] = packetId & 0x00FF; // LSB for packet ID

        sendUnsubscribe[i++] = stringLength & 0xFF00;   // MSB for string length
        sendUnsubscribe[i++] = stringLength & 0x00FF;   // LSB for string length

        for (x = 0; x < strlen(unsubTopicFilter); x++)
        {
            sendUnsubscribe[i++] = unsubTopicFilter[x]; // store unsubscribe topic filter
        }
        sendUnsubscribe[1] = i - 2; // calculating remaining length for 1 byte
        return i;   //return total length
    }
    else
    {
        sendUnsubscribe[i++] = 0; // MSB for packet ID 
        sendUnsubscribe[i++] = 0; // LSB for packet ID
        return 65535; // if could not find packet ID, return 65535
    }
}

uint8_t getPacketIdIndex(char *topicFilter)
{
    uint8_t x = 0;
    for (x = 0; x < INDEX; x++)
    {
        if (strcmp(topicFilter, filterTopics[x]) == 0) // check if topic matches, value in array
        {
            if (packetIDArray[x] >= 0)
            {
                return x;   // if topic matches, and packet id greater than 0, return packet id index
            }
        }
    }
    return 101;
}

uint16_t sendDisconnect(uint8_t *sendDisconnect)
{
    uint16_t i = 0;

    sendDisconnect[i++] = MQTT_CTL_DISCONNECT; // MQTT Disconnect command
    sendDisconnect[i++] = 0;               // remaining length
    return i;
}

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

// uint16_t packetIdCheck = packetIDArray[INDEX];  //
//     char stringCheck[100];

//     strcpy(stringCheck, filterTopics[INDEX]);