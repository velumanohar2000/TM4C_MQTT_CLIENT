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

#ifndef TCP_H_
#define TCP_H_

#include <stdint.h>
#include <stdbool.h>
#include "ip.h"

typedef struct _tcpHeader // 20 or more bytes
{
  uint16_t sourcePort;
  uint16_t destPort;
  uint32_t sequenceNumber;
  uint32_t acknowledgementNumber;
  uint16_t offsetFields;
  uint16_t windowSize;
  uint16_t checksum;
  uint16_t urgentPointer;
  uint8_t data[0];
} tcpHeader;

// TCP states
#define TCP_CLOSED 0
#define TCP_LISTEN 1
#define TCP_SYN_RECEIVED 2
#define TCP_SYN_SENT 3
#define TCP_ESTABLISHED 4
#define TCP_FIN_WAIT_1 5
#define TCP_FIN_WAIT_2 6
#define TCP_CLOSING 7
#define TCP_CLOSE_WAIT 8
#define TCP_LAST_ACK 9
#define TCP_TIME_WAIT 10
#define TCP_LAST_FIN_ACK 11

// MQTT states
#define MQTT_CONNECT 0
#define MQTT_CONACK_ACK 1
#define MQTT_PUBLISH 2
#define MQTT_SUB 3
#define MQTT_PUBACK_ACK 5
#define MQTT_WAIT 6
#define MQTT_SUBSCRIBE 7
#define MQTT_SUBACK_ACK 8
#define MQTT_KEEP_ALIVE 9
#define MQTT_UNSUBSCRIBE 10
#define MQTT_DISCONNECT 11
#define MQTT_UNSUBACK_ACK 12
#define MQTT_PACKETACK_ACK 13
// TCP offset/flags
#define FIN 0x0001
#define SYN 0x0002
#define RST 0x0004
#define PSH 0x0008
#define ACK 0x0010
#define URG 0x0020
#define ECE 0x0040
#define CWR 0x0080
#define NS 0x0100
#define OFS_SHIFT 12

uint8_t TCP_STATE;
uint8_t MQTT_STATE;
int8_t HANDSHAKE_STATE;

char publishTopic[80];
char publishData[100];
char subTopicFilter[80];
char unsubTopicFilter[80];

bool sendUnsubFlag;
bool sendPubFlag;
bool sendConnectFlag;
bool sendArpFlag;
bool sendFinFlag;
bool recievedTCP;
bool sendSubFlag;

bool isPUB;
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void tcpSetState(uint8_t state);
uint8_t tcpGetState();
void stateMachine(etherHeader *ether, socket *s);

bool tcpIsSyn(etherHeader *ether);
bool tcpIsAck(etherHeader *ether);

bool isTcp(etherHeader *ether);

// TODO: Add functions here
void saveSeqAck(socket *s);
void getTcpMessageSocket(etherHeader *ether, socket *s);
void getSeqACK(etherHeader *ether,socket *s);
void sendTcpMessage(etherHeader *ether, socket s, uint8_t data[], uint16_t dataSize);
void sendMqttConnect(etherHeader *ether, socket *s);
void sendMqttPub(etherHeader *ether, socket *s);
void sendMqttSub(etherHeader *ether, socket *s);
void sendMqttUnsub(etherHeader *ether, socket *s);
void sendMqttDisconnect(etherHeader *ether, socket *s);
void printToUart(etherHeader *ether, socket *s);


#endif
