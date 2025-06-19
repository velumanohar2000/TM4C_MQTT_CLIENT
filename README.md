# IoT MQTT Ethernet Stack

This project implements a minimal IP networking stack and MQTT client for the
Texas Instruments TM4C123GH6PM microcontroller using an ENC28J60 Ethernet
controller.  The code is intended for Code Composer Studio and provides
examples of publishing and subscribing to MQTT topics directly from an
embedded device.

## Features

- **Ethernet driver** for the ENC28J60 SPI controller (`eth0.c`)
- **ARP, IP, ICMP, UDP and TCP** protocol implementations (`arp.c`, `ip.c`,
  `icmp.c`, `udp.c`, `tcp.c`)
- **MQTT client** capable of CONNECT, PUBLISH, SUBSCRIBE, UNSUBSCRIBE and
  DISCONNECT operations (`mqtt.c`)
- **UART command shell** for configuring network parameters and interacting
  with the MQTT client. Available commands include:
  - `ifconfig` – display current MAC and IP settings
  - `set ip|gw|dns|time|mqtt|sn w.x.y.z` – store network addresses in EEPROM
  - `connect`/`disconnect` – open or close the TCP/MQTT session
  - `pub <topic> <data>` – publish a message
  - `sub <topic>` and `unsub <topic>` – subscribe or unsubscribe
  - `status` – display TCP/MQTT state
- **Persistent configuration** stored in on‑chip EEPROM (`eeprom.c`)
- **Peripheral drivers** for UART0, SPI0, timers and GPIO
- Example main loop in `ethernet.c` showing ARP handling, TCP state machine and
  MQTT message processing

## Hardware

- TI TM4C123GH6PM (Tiva C LaunchPad)
- ENC28J60 Ethernet controller on SPI0
- UART0 used for the serial command interface

This repository demonstrates how an embedded system can implement its own
network stack and communicate with an MQTT broker without a heavyweight
operating system.
