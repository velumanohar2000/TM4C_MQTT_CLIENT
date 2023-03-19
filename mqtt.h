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

#ifndef MQTT_H_
#define MQTT_H_

#include <stdint.h>
#include <stdbool.h>
#include "tcp.h"

int32_t packetIDArray[100];
char filterTopics[100][80];

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

typedef struct _MQTT_FIXED // 20 or more bytes
{
  uint8_t mqttControlType;
  // uint8_t remainingLengthMSB;
  uint8_t remainingLengthLSB;
  uint8_t data[0];
} MQTT_FIXED;

typedef struct _CONNECT_VARIABLE // 20 or more bytes
{
  uint8_t lengthMSB;
  uint8_t lengthLSB;
  char protocalName[4];
  uint8_t level;
  uint8_t connectFlags;
  uint8_t keepAliveMSB;
  uint8_t keepAliveLSB;
  uint8_t data[0];

} CONNECT_VARIABLE;

typedef struct _PAYLOAD
{
  uint8_t lengthMSB_LSB;
  char clientId[127]; // need to change
} PAYLOAD;

//-------------------------------------------------------------------------------------------------------------

typedef struct _PUBLISH_VARIABLE
{
  uint8_t lengthMSB;
  uint8_t lengthLSB;
  char topicName[4];
  uint8_t packetIdMSB;
  uint8_t packetIdLSB;
  uint8_t data[0];

} PUBLISH_VARIABLE;

typedef struct _PUBLISH_PAYLOAD // 20 or more bytes
{
  uint16_t lengthTopic;
  char data[0]; // need to change

} PUBLISH_PAYLOAD;





uint16_t sendConnect(uint8_t *sendCon, char *clientId);
uint16_t sendPublish(char *topic, char *data, uint8_t *sendPub);
uint16_t sendSub(char *topicFilter, uint8_t *sendSub, uint16_t packetId);

uint16_t sendUnsubFunc(char *unsubTopicFilter, uint8_t *sendUnsubscribe);

uint8_t getPacketIdIndex(char *topicFilter);

uint16_t sendDisconnect(uint8_t *sendDisconnect);
 


//
#endif
