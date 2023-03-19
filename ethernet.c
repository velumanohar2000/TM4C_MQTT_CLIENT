// Ethernet Example
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL w/ ENC28J60
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// ENC28J60 Ethernet controller on SPI0
//   MOSI (SSI0Tx) on PA5

//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS (SW controlled) on PA3
//   WOL on PB3
//   INT on PC6

// Pinning for IoT projects with wireless modules:
// N24L01+ RF transceiver
//   MOSI (SSI0Tx) on PA5
//   MISO (SSI0Rx) on PA4
//   SCLK (SSI0Clk) on PA2
//   ~CS on PE0
//   INT on PB2
// Xbee module
//   DIN (UART1TX) on PC5
//   DOUT (UART1RX) on PC4

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "tm4c123gh6pm.h"
#include "clock.h"
#include "eeprom.h"
#include "gpio.h"
#include "spi0.h"
#include "uart0.h"
#include "wait.h"
#include "timer.h"
#include "eth0.h"
#include "arp.h"
#include "ip.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"
#include "mqtt.h"

// Pins
#define RED_LED PORTF, 1
#define BLUE_LED PORTF, 2
#define GREEN_LED PORTF, 3
#define PUSH_BUTTON PORTF, 4

// EEPROM Map
#define EEPROM_DHCP 1
#define EEPROM_IP 2
#define EEPROM_SUBNET_MASK 3
#define EEPROM_GATEWAY 4
#define EEPROM_DNS 5
#define EEPROM_TIME 6
#define EEPROM_MQTT 7
#define EEPROM_ERASED 0xFFFFFFFF

// Global values
// uint8_t TCP_STATE = 0;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    enablePort(PORTF);
    _delay_cycles(3);

    // Configure LED and pushbutton pins
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(BLUE_LED);
    selectPinDigitalInput(PUSH_BUTTON);
    enablePinPullup(PUSH_BUTTON);
}

void displayConnectionInfo()
{
    uint8_t i;
    char str[10];
    uint8_t mac[6];
    uint8_t ip[4];
    getEtherMacAddress(mac);
    putsUart0("  HW:    ");
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%02" PRIu8, mac[i]);
        putsUart0(str);
        if (i < HW_ADD_LENGTH - 1)
            putcUart0(':');
    }
    putcUart0('\n');
    getIpAddress(ip);
    putsUart0("  IP:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putsUart0(" (static)");
    putcUart0('\n');
    getIpSubnetMask(ip);
    putsUart0("  SN:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpGatewayAddress(ip);
    putsUart0("  GW:    ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpDnsAddress(ip);
    putsUart0("  DNS:   ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpTimeServerAddress(ip);
    putsUart0("  Time:  ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    getIpMqttBrokerAddress(ip);
    putsUart0("  MQTT:  ");
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        snprintf(str, sizeof(str), "%" PRIu8, ip[i]);
        putsUart0(str);
        if (i < IP_ADD_LENGTH - 1)
            putcUart0('.');
    }
    putcUart0('\n');
    if (isEtherLinkUp())
        putsUart0("Link is up\n");
    else
        putsUart0("Link is down\n");
}

void readConfiguration()
{
    uint32_t temp;
    uint8_t *ip;

    temp = readEeprom(EEPROM_IP);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpAddress(ip);
    }
    temp = readEeprom(EEPROM_SUBNET_MASK);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpSubnetMask(ip);
    }
    temp = readEeprom(EEPROM_GATEWAY);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpGatewayAddress(ip);
    }
    temp = readEeprom(EEPROM_DNS);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpDnsAddress(ip);
    }
    temp = readEeprom(EEPROM_TIME);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpTimeServerAddress(ip);
    }
    temp = readEeprom(EEPROM_MQTT);
    if (temp != EEPROM_ERASED)
    {
        ip = (uint8_t *)&temp;
        setIpMqttBrokerAddress(ip);
    }
}

#define MAX_CHARS 80
char strInput[MAX_CHARS + 1];
char *token;
uint8_t count = 0;

uint8_t asciiToUint8(const char str[])
{
    uint8_t data;
    if (str[0] == '0' && tolower(str[1]) == 'x')
        sscanf(str, "%hhx", &data);
    else
        sscanf(str, "%hhu", &data);
    return data;
}

void processShell(etherHeader *data, socket s)
{

    bool end;
    char c;
    uint8_t i;
    uint8_t ip[IP_ADD_LENGTH];

    uint32_t *p32;

    if (kbhitUart0())
    {
        c = getcUart0();

        end = (c == 13) || (count == MAX_CHARS);
        if (!end)
        {
            if ((c == 8 || c == 127) && count > 0)
                count--;
            if (c >= ' ' && c < 127)
                strInput[count++] = c;
        }
        else
        {
            strInput[count] = '\0';
            count = 0;
            token = strtok(strInput, " ");
            if (strcmp(token, "ifconfig") == 0)
            {
                displayConnectionInfo();
            }
            else if (strcmp(token, "reboot") == 0)
            {
                NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
            }
            else if (strcmp(token, "set") == 0)
            {
                token = strtok(NULL, " ");
                if (strcmp(token, "ip") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpAddress(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_IP, *p32);
                }
                else if (strcmp(token, "sn") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpSubnetMask(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_SUBNET_MASK, *p32);
                }
                else if (strcmp(token, "gw") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpGatewayAddress(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_GATEWAY, *p32);
                }
                else if (strcmp(token, "dns") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpDnsAddress(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_DNS, *p32);
                }
                else if (strcmp(token, "time") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpTimeServerAddress(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_TIME, *p32);
                }

                else if (strcmp(token, "mqtt") == 0)
                {
                    for (i = 0; i < IP_ADD_LENGTH; i++)
                    {
                        token = strtok(NULL, " .");
                        ip[i] = asciiToUint8(token);
                    }
                    setIpMqttBrokerAddress(ip);
                    p32 = (uint32_t *)ip;
                    writeEeprom(EEPROM_MQTT, *p32);
                }
            }

            if (strcmp(token, "connect") == 0 || strcmp(token, "con") == 0)
            {
                // TCP_STATE = TCP_CLOSED;
                if (TCP_STATE == TCP_ESTABLISHED)
                {
                    // sendConnectFlag = 1;
                    MQTT_STATE = MQTT_CONNECT;
                    stateMachine(data, &s);
                    // sendConnectFlag = 0;
                }
            }

            else if (strcmp(token, "arp") == 0 || (strcmp(token, "connect") == 0 || strcmp(token, "con") == 0))
            {
                HANDSHAKE_STATE = 0;
                //                getIpAddress(myIP);
                //                getIpTimeServerAddress(remoteIp);
                //                sendArpRequest(data, myIP, remoteIp);
            }

            else if (strcmp(token, "fin") == 0)
            {
                TCP_STATE = TCP_FIN_WAIT_1;
                stateMachine(data, &s);
            }
            else if (strcmp(token, "disconnect") == 0 || strcmp(token, "dis") == 0)
            {
                MQTT_STATE = MQTT_DISCONNECT;
                stateMachine(data, &s);
            }
            else if (strcmp(token, "pub") == 0 || strcmp(token, "publish") == 0)
            {

                token = strtok(NULL, " ");
                strcpy(publishTopic, token);
                token = strtok(NULL, "\0");
                strcpy(publishData, token);
                sendPubFlag = 1;
                MQTT_STATE = MQTT_PUBLISH;
                // sendMqttPub(data,&s);
                stateMachine(data, &s);
                sendPubFlag = 0;
            }
            else if (strcmp(token, "sub") == 0) //|| strcmp(token, "subscribe") == 0)
            {

                token = strtok(NULL, " ");
                strcpy(subTopicFilter, token);
                sendSubFlag = 1;
                MQTT_STATE = MQTT_SUBSCRIBE;
                stateMachine(data, &s);
                sendSubFlag = 0;
            }
            else if (strcmp(token, "unsub") == 0) //|| strcmp(token, "unsubscribe") == 0)
            {

                token = strtok(NULL, " ");
                strcpy(unsubTopicFilter, token);
                MQTT_STATE = MQTT_UNSUBSCRIBE;
                stateMachine(data, &s);
            }
            else if (strcmp(token, "help") == 0)
            {
                putsUart0("Commands:\r");
                putsUart0("  ifconfig\r");
                putsUart0("  reboot\r");
                putsUart0("  set ip|gw|dns|time|mqtt|sn w.x.y.z\r");
            }
        }
    }
}

void fillSocket(etherHeader *ether, socket *s, uint8_t *remoteIp)
{
    int i;
    arpPacket *arp = (arpPacket *)ether->data;
    for (i = 0; i < IP_ADD_LENGTH; i++)
    {
        s->remoteIpAddress[i] = arp->sourceIp[i];
    }
    for (i = 0; i < HW_ADD_LENGTH; i++)
    {
        s->remoteHwAddress[i] = arp->sourceAddress[i];
    }

    s->remotePort = 1883;
    s->localPort = 65530;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

// understand:
// isEtherDataAvailable()
// isEtherOverflow()
// getEtherPacket()

// Max packet is calculated as:
// Ether frame header (18) + Max MTU (1500) + CRC (4)

#define MAX_PACKET_SIZE 1522

int main(void)
{
    bool receivedDataOnce = false;
    sendUnsubFlag = 0;
    sendSubFlag = 0;
    sendFinFlag = 0;
    sendPubFlag = 0;
    sendArpFlag = 1;
    HANDSHAKE_STATE = 0;
    // TCP_STATE = TCP_CLOSED;
    //  bool badTCP;
    uint8_t *udpData; //, *tcpData;
    // uint8_t badTCP = false;
    uint8_t buffer[MAX_PACKET_SIZE];
    etherHeader *data = (etherHeader *)buffer; // buffer is an array of bytes but we are interpreting it as ether data struct
    socket s;

    // Init controller
    initHw();

    // Setup UART0
    initUart0();
    setUart0BaudRate(115200, 40e6);

    // Init timer
    initTimer();

    // Init ethernet interface (eth0)
    // Use the value x from the spreadsheet
    putsUart0("\nStarting eth0\n");
    initEther(ETHER_UNICAST | ETHER_BROADCAST | ETHER_HALFDUPLEX);
    setEtherMacAddress(2, 3, 4, 5, 6, 0x6F);

    // Init EEPROM
    initEeprom();
    readConfiguration();

    setPinValue(GREEN_LED, 1);
    waitMicrosecond(100000);
    setPinValue(GREEN_LED, 0);
    waitMicrosecond(100000);
    // need to set to 4096 on project properties

    // Main Loop
    // RTOS and interrupts would greatly improve this code,
    // but the goal here is simplicity
    uint8_t myIP[4];
    uint8_t remoteIp[4];

    while (true)
    {
        // badTCP = false;
        recievedTCP = false;
        // sendTcpMeassage = false;
        //  Put terminal processing here
        processShell(data, s);
        if (receivedDataOnce)
        {
            if (HANDSHAKE_STATE == 0)
            {
                getIpAddress(myIP);
                getIpTimeServerAddress(remoteIp);
                sendArpRequest(data, myIP, remoteIp);
            }
            else if (HANDSHAKE_STATE == 1 && TCP_STATE == TCP_ESTABLISHED)
            {

                MQTT_STATE = MQTT_CONNECT;
                HANDSHAKE_STATE = -1;
                stateMachine(data, &s);
            }
        }

        // Packet processing
        if (isEtherDataAvailable()) //|| sendTcpMeassage)
        {

            if (isEtherOverflow())
            {
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 1);
                waitMicrosecond(100000);
                setPinValue(RED_LED, 0);
            }

            // Get packet
            getEtherPacket(data, MAX_PACKET_SIZE);

            //             Handle ARP request, type 806
            //             Handle ARP request
            if (isArpRequest(data))
            {
                sendArpResponse(data);
                // badTCP = true;
            }
            // make tcpSocket and process, cast as arp packet to read data
            if (isArpResponse(data))
            {
                if (HANDSHAKE_STATE == 0)
                {
                    TCP_STATE = TCP_CLOSED;
                    fillSocket(data, &s, remoteIp);
                    HANDSHAKE_STATE = 1;
                    stateMachine(data, &s);

                    // sendTcpMessage(data, s, NULL, 0);
                }
            }

            // Handle IP datagram, type 800
            if (isIp(data))
            {

                // is someone trying to reach me?
                if (isIpUnicast(data))
                {
                    // Handle ICMP ping request
                    // take the request, flip a few fields and send it back out
                    // saves memory and improves performance
                    if (isPingRequest(data))
                    {
                        sendPingResponse(data);
                    }

                    // Handle UDP datagram
                    else if (isUdp(data))
                    {
                        udpData = getUdpData(data);
                        if (strcmp((char *)udpData, "on") == 0)
                            setPinValue(GREEN_LED, 1);
                        if (strcmp((char *)udpData, "off") == 0)
                            setPinValue(GREEN_LED, 0);
                        getUdpMessageSocket(data, &s);
                        sendUdpMessage(data, s, (uint8_t *)"Received", 9);
                    }

                    // Handle TCP datagram
                    else if (isTcp(data)) //&& !badTCP)
                    {
                        putsUart0("\n\nIT IS TCP\n");
                        ipHeader *ip = (ipHeader *)data->data;
                        uint8_t ipHeaderLength = ip->size * 4;
                        tcpHeader *tcp = (tcpHeader *)((uint8_t *)ip + ipHeaderLength);
                        uint16_t recievedOffsetField = ntohs(tcp->offsetFields) & 0x3F;

                        MQTT_FIXED *mqttFixed = (MQTT_FIXED *)tcp->data;

                        if ((recievedOffsetField & ACK) == ACK)
                        {
                            putsUart0("ACK PRESENT\n");
                            getSeqACK(data, &s);
                            saveSeqAck(&s);
                        }
                        if ((recievedOffsetField == (SYN | ACK)))
                        {
                            putsUart0("SYN and ACK PRESENT\n");

                            TCP_STATE = TCP_SYN_SENT;
                            stateMachine(data, &s);
                        }
                        else if ((recievedOffsetField == (FIN | ACK)))
                        {
                            putsUart0("FIN and ACK PRESENT\n");

                            if (TCP_STATE == TCP_FIN_WAIT_1)
                            {
                                TCP_STATE = TCP_FIN_WAIT_2;
                                stateMachine(data, &s);
                            }
                            else
                            {
                                TCP_STATE = TCP_LAST_FIN_ACK;
                                stateMachine(data, &s);
                            }
                        }
                        else if ((TCP_STATE == TCP_LAST_FIN_ACK) && (recievedOffsetField & ACK == ACK))
                        {
                            TCP_STATE = 20;
                            // stateMachine(data, &s);
                        }

                        else if (TCP_STATE == TCP_ESTABLISHED) //&& TCP_STATE == TCP_ESTABLISHED)
                        {
                            uint8_t state = TCP_STATE;
                            putsUart0("PSH and ACK PRESENT\n");

                            if (mqttFixed->mqttControlType == 0x20) //&& (recievedOffsetField & (ACK | PSH)) == (ACK | PSH) )
                            {
                                putsUart0("IT IS CONNACK\n");
                                //  TCP_STATE = TCP_ESTABLISHED;
                                MQTT_STATE = MQTT_CONACK_ACK;
                                stateMachine(data, &s);
                            }
                            else if (mqttFixed->mqttControlType == 0x90) //&& (recievedOffsetField & (ACK | PSH)) == (ACK | PSH) )
                            {
                                putsUart0("IT IS SUBACK\n");
                                MQTT_STATE = MQTT_SUBACK_ACK;
                                stateMachine(data, &s);
                            }
                            else if (mqttFixed->mqttControlType == 0x30) //&& (recievedOffsetField & (ACK | PSH)) == (ACK | PSH) )
                            {
                                putsUart0("IT IS PUBACK\n");
                                MQTT_STATE = MQTT_PUBACK_ACK;
                                stateMachine(data, &s);
                            }
                            else if (mqttFixed->mqttControlType == 0xB0) //&& (recievedOffsetField & (ACK | PSH)) == (ACK | PSH) )
                            {
                                putsUart0("IT IS UNSUBACK\n");
                                MQTT_STATE = MQTT_UNSUBACK_ACK;
                                stateMachine(data, &s);
                            }
                        }
                    }
                }
            }
        }
        receivedDataOnce = true;
    }
}

//}

//        else if (sendConnectFlag && TCP_STATE == 17)
//        {
//            TCP_STATE = TCP_ESTABLISHED;
//            MQTT_STATE = MQTT_CONNECT;
//            stateMachine(data, &s);
//            sendConnectFlag = 0;
//        }
//        else if (sendFinFlag)
//        {
//            TCP_STATE = TCP_FIN_WAIT_1;
//            stateMachine(data, &s);
//            sendFinFlag = 0;
//        }
//        else if (sendPubFlag)
//        {
//            //TCP_STATE = TCP_ESTABLISHED;
//            MQTT_STATE = MQTT_PUBLISH;
//            //sendMqttPub(data,&s);
//            stateMachine(data,&s);
//            sendPubFlag = 0;
//        }
//        else if (sendSubFlag)
//        {
//            //TCP_STATE = TCP_ESTABLISHED;
//            MQTT_STATE = MQTT_SUBSCRIBE;
//            stateMachine(data,&s);
//            sendSubFlag = 0;
//        }
//        else if (sendUnsubFlag)
//        {
//            //TCP_STATE = TCP_ESTABLISHED;
//            MQTT_STATE = MQTT_UNSUBSCRIBE;
//            stateMachine(data,&s);
//            sendUnsubFlag = 0;
//        }
//        else
//        {}
