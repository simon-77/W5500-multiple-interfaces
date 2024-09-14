/*
 * Example.ino
 * 
 * Example usage of the "W5500" class, conecting multiple ethernet modules to one Arduino.
 * 
 * This example demonstrates how to configure and use two W5500 Ethernet interfaces
 * in different IP networks. It also shows how to monitor the status of the interfaces & sockets.
 * 
 *===========================
 *      Usage Scenario
 * The W5500 ethernet interfaces (eth1 & eth2) are connected to two different networks.
 * In each network is another device (client1 & client2), between which the Arduino relays data on different sockets.
 
 * *** Socket 0:
 *      - eth1: TCP Server listening on port 1234
 *      - eth2: TCP Client connecting to <client2>:1234
 * The Arduino relays data between the two sockets and the Serial connection.
 * Status changes of the sockets are monitored and printed to the Serial Monitor.
 * 
 * *** Socket 1:
 *      - eth1: UDP on port 3210, with <client1>:3210 as destination
 *      - eth2: UDP on port 3210, with <client2>:3210 as destination
 * The Arduino relays data between these two sockets. Use `nc -u <Arduino-IP> 3210 -p 3210` on a linux client for testing.
 * The received data is printed to the Serial Monitor as well.
 * 
 * *** Socket 2: (ssh)
 *      - eth1: TCP Server listening on port 22
 *      - eth2: TCP Client connecting to <client2>:22
 * Only if a client connects to eth1, the Arduino will open a connection to <client2>:22 and relay the data.
 * E.g. client 1 can use `ssh` to connect to the Arduino IP, but the connection will be relayed to client 2.
 * 
 *===========================
 *      Usage Instructions
 * 1. Copy all files into a folder named "Example".
 * 2. Open the "Example.ino" file in the Arduino IDE, select your board and upload the sketch.
 * 3. Open the Serial Monitor (baud 115200) to view the output, otherwise the sketch will not run.
 * 
 * Folder Structure:
 * Example/
 * ├── Example.ino
 * ├── SpiFrame.cpp
 * ├── SpiFrame.h
 * ├── W5500.cpp
 * ├── W5500.h
 * 
 * Hardware Setup:
 * - Connect the W5500 Ethernet modules to the SPI pins of your microcontroller.
 * - Ensure that the CS (Chip Select) pins are connected to the appropriate pins as defined in the sketch.
 * 
 * Author: Simon Aster
 * Date: 2024-08-30
 * License: GPL v3
 * 
 * Tested with Arduino MKR Zero and 2 "MKR ETH Shield" modules.
 */

#include "W5500.h"
#include "SpiFrame.h"


// SPI Frame
SpiFrame spi_eth1(5);
W5500 eth1(spi_eth1);

SpiFrame spi_eth2(4);
W5500 eth2(spi_eth2);

// MAC Addresse
// To stay compliant with the IEEE 802c standard, set the 2nd hex-digit to "2" (locally administered)
W5500::MAC_t eth1_mac = {0x02, 0x00, 0x00, 0x00, 0xAF, 0xFE};
W5500::MAC_t eth2_mac = {0xF2, 0xFF, 0xDE, 0xAD, 0xBE, 0xEF};


// Network Configuration
const W5500::IP_t eth1_ip = {172, 26, 123, 16};
const W5500::IP_t eth1_subnet = {255, 255, 255, 0};
const W5500::IP_t eth1_gateway = {172, 26 ,123, 1};

const W5500::IP_t eth2_ip = {192, 168, 177, 18};
const W5500::IP_t eth2_subnet = {255, 255, 255, 0};
const W5500::IP_t eth2_gateway = {192, 168, 177, 1};


// Client 1 & 2 IP addresses (connected to eth1 & eth2 respectively)
const W5500::IP_t client1_ip = {172, 26, 123, 20};
const W5500::IP_t client2_ip = {192, 168, 177, 10};

const uint16_t socket0_port = 1234;
const uint16_t socket1_port = 3210;
const uint16_t socket2_port = 22;


// Helper Functions for nicely formatted Serial prints
void SerialPrintIP(W5500::IP_t ip);
void SerialPrintMAC(W5500::MAC_t mac);
void SerialPrintSocketStatus(W5500::SocketStatus status);


//############################################################################
//                                 Setup
//############################################################################

void setup()
{
    // Wait for serial connection before continuing
	Serial.begin(115200);
    Serial.setTimeout(1); // for reading with Serial.readBytes(buffer-size)
    while(!Serial);
    Serial.println("Starting setup ...");
    
    //===================================================================
    //=== INTERFACE Configuration

    // Configure Interface eth1
    eth1.init();
    eth1.setInterfaceMAC(eth1_mac);
    eth1.setInterfaceNetwork(eth1_ip, eth1_subnet, eth1_gateway);

    // Configure Interface eth2
    eth2.init();
    eth2.setInterfaceNetwork(eth2_ip, eth2_subnet, eth2_gateway);
    eth2.setInterfaceMAC(eth2_mac);

    // Wait until both links are "up" (Phy-Status bit 0: '1' = link up)
    while((eth1.phyStatus() & eth2.phyStatus() & 0x01) == 0){
        delay(500);
        Serial.print("PHY Status - eth1: ");
        Serial.print(eth1.phyStatus(), BIN);
        Serial.print(" ; eth2: ");
        Serial.println(eth2.phyStatus(), BIN);
    }

    //===================================================================
    //=== SOCKET Configuration


    //=== Socket 0 - TCP Server (eth1) & TCP Client (eth2) - with status check's & serial prints

    // ETH1: Socket 0 - TCP Server on port 1234
    eth1.setSocketSource(0, socket0_port);
    Serial.print("eth1 - socket 0 opening as TCP server ... ");
    if(eth1.socketOpen(0, W5500::TCP_Server)){
        Serial.println("opened successfully");
    } else {
        Serial.println("failed to open socket");
    }

    // ETH2: Socket 0 - TCP Client connecting to IP & Port
    eth2.setSocketDest(0, client2_ip, socket0_port); // destination IP & port of another TCP server to connect to
    eth2.setSocketSource(0, 5050); // source port needs to be set as well, can be arbitrary usually
    Serial.print("eth2 - socket 0 connecting as TCP client ... ");
    if(eth2.socketOpen(0, W5500::TCP_Client)){
        Serial.println("connected successfully");
    } else {
        Serial.println("failed to connect to server");
    }


    //=== Socket 1 - UDP on port 3210 each

    eth1.setSocketSource(1, socket1_port);
    eth1.setSocketDest(1, client1_ip, socket1_port);
    eth1.socketOpen(1, W5500::UDP);

    eth2.setSocketSource(1, socket1_port);
    eth2.setSocketDest(1, client2_ip, socket1_port);
    eth2.socketOpen(1, W5500::UDP);


    //=== Socket 2 - TCP port 22 (ssh): forward from eth1 to eth2

    // open port 22 on eth1
    eth1.setSocketSource(2, socket2_port);
    eth1.socketOpen(2, W5500::TCP_Server);

    // set forwarding Address, but don't open the socket until eth1 is connected
    eth2.setSocketDest(2, client2_ip, socket2_port);
    eth2.setSocketSource(2, 5051); // source port needs to be set as well, can be arbitrary usually


    //===================================================================
    //=== Read back MAC & IP addresses for printing to Serial

    // Local variables
    W5500::MAC_t mac_read;
    W5500::IP_t ip_read;

    // Read back the MAC addresses and print them
    eth1.regInterfaceAddress(W5500::SourceMAC, false, mac_read, sizeof(mac_read));
    Serial.print("MAC Address (eth1): ");
    SerialPrintMAC(mac_read);
    eth2.regInterfaceAddress(W5500::SourceMAC, false, mac_read, sizeof(mac_read));
    Serial.print(" ; (eth2): ");
    SerialPrintMAC(mac_read);
    Serial.println();
    
    // eth1, socket 0 - print listening IP & Port
    eth1.regInterfaceAddress(W5500::SourceIP, false, ip_read, sizeof(ip_read));
    Serial.print("eth1 - listening on: ");
    SerialPrintIP(ip_read);
    Serial.print(":");
    Serial.println(eth1.getSocketPort(0, W5500::SourcePort));
    
    // eth2, socket 0 - print destination IP & Port
    eth2.regSocketAddress(0, W5500::DestinationIP, false, ip_read, sizeof(ip_read));
    Serial.print("eth 2 - connecting to: ");
    SerialPrintIP(ip_read);
    Serial.print(":");
    Serial.println(eth2.getSocketPort(0, W5500::DestinationPort));

}
// end of setup

//############################################################################
//                                  Main Loop
//############################################################################

void loop()
{
    // Software buffer for data relay
    uint8_t buffer[1000];

    //===================================================================
    //=== Socket 0 - Track status and print verbose status changes

    static W5500::SocketStatus status_eth1_0 = W5500::Closed;
    static W5500::SocketStatus status_eth2_0 = W5500::Closed;
    W5500::SocketStatus new_status;

    // Print the status of the sockets when they change
    new_status = eth1.socketStatus(0);
    if (status_eth1_0 != new_status) {
        status_eth1_0 = new_status;
        Serial.print("eth1 (socket 0) - status change: ");
        SerialPrintSocketStatus(status_eth1_0);
        Serial.println();
    }
    new_status = eth2.socketStatus(0);
    if (status_eth2_0 != new_status) {
        status_eth2_0 = new_status;
        Serial.print("eth2 (socket 0) status change: ");
        SerialPrintSocketStatus(status_eth2_0);
        Serial.println();
    }

    // Keep the sockets open, try to reconnect if they are closed
    eth1.socketKeepOpen(0, W5500::TCP_Server);
    if(eth1.socketStatus(0) == W5500::TCP_Connected) {
        // try connecting to <client2> only if eth1 is connected
        eth2.socketKeepOpen(0, W5500::TCP_Client);
    }

    // Relay data between the two sockets & the Serial connection
    if (eth1.receiveAvailable(0) > 0) {
        // reaad data from eth1
        int len = eth1.receive(0, buffer, sizeof(buffer));
        // print to Serial
        Serial.print("eth1 (socket 0) - TCP received: ");
        Serial.write(buffer, len);
        Serial.println();
        // send to eth2
        eth2.send(0, buffer, len);
    }

    if (eth2.receiveAvailable(0) > 0) {
        // read data from eth2
        int len = eth2.receive(0, buffer, sizeof(buffer));
        // print to Serial
        Serial.print("eth2 (socket 0) - TCP received: ");
        Serial.write(buffer, len);
        Serial.println();
        // send to eth1
        eth1.send(0, buffer, len);
    }

    if (Serial.available() > 0) {
        // read data from Serial
        int len = Serial.readBytes(buffer, sizeof(buffer));

        // need a 2nd copy for eth2, as the write function will modify the buffer
        uint8_t buffer2[sizeof(buffer)];
        memcpy(buffer2, buffer, len);

        // send to eth1 & eth2
        eth1.send(0, buffer, len);
        eth2.send(0, buffer2, len);
    }
    
    //===================================================================
    //=== Socket 1 - UDP relay between eth1 & eth2

    eth1.socketKeepOpen(1, W5500::UDP);
    eth2.socketKeepOpen(1, W5500::UDP);

    // Relay data between eth1 & eth2
    if (eth1.receiveAvailable(1) > 0) {
        int len = eth1.receive(1, buffer, sizeof(buffer), UdpHeaderMode::UpdateDestination);
        Serial.print("eth1 (socket 1) - UDP received : ");
        Serial.write(buffer, len);
        eth2.send(1, buffer, len);
        
    }
    if (eth2.receiveAvailable(1) > 0) {
        int len = eth2.receive(1, buffer, sizeof(buffer), UdpHeaderMode::UpdateDestination);
        Serial.print("eth2 (socket 1) - UDP received : ");
        Serial.write(buffer, len);
        eth1.send(1, buffer, len);
    }

    //===================================================================
    //=== Socket 2 - Forwarding TCP Port 22 (ssh) from eth1 to eth2

    // Keep TCP server (eth1) open, but only connect eth2 if the someone connects to eth1 (TCP server)
    eth1.socketKeepOpen(2, W5500::TCP_Server);
    switch(eth1.socketStatus(2)){
        case W5500::Closed:
        case W5500::TCP_Listen:
            // no connection on eth1 -> close connection on eth2
            eth2.socketClose(2);
            break;
        case W5500::TCP_Connected:
            // active connection on eth1 -> open connection on eth2
            eth2.socketKeepOpen(2, W5500::TCP_Client); // won't harm if already open
            break;
        case W5500::Temporary:
        default:
            // do nothing
            break;
    }

    // Relay data between eth1 & eth2, only if both are connected
    if (eth1.socketConnected(2) && eth2.socketConnected(2)) {
        if (eth1.receiveAvailable(2) > 0) {
            int len = eth1.receive(2, buffer, sizeof(buffer));
            eth2.send(2, buffer, len);
        }
        if (eth2.receiveAvailable(2) > 0) {
            int len = eth2.receive(2, buffer, sizeof(buffer));
            eth1.send(2, buffer, len);
        }
    }
}
// end of loop


//############################################################################
//                                 Helper Functions
//############################################################################


void SerialPrintIP(W5500::IP_t ip) {
    for (int i = 0; i < sizeof(W5500::IP_t); i++) {
        Serial.print(ip[i]);
        if (i < sizeof(W5500::IP_t) - 1) {
            Serial.print(".");
        }
    }
}

void SerialPrintMAC(W5500::MAC_t mac) {
    char hex_byte[3];
    for (int i = 0; i < sizeof(W5500::MAC_t); i++) {
        snprintf(hex_byte, sizeof(hex_byte), "%02X", mac[i]);
        Serial.print(hex_byte);
        if (i < sizeof(W5500::MAC_t) - 1) {
            Serial.print(":");
        }
    }
}

void SerialPrintSocketStatus(W5500::SocketStatus status) {
    switch (status) {
        case W5500::Closed:
            Serial.print("Closed");
            break;
        case W5500::UDP_Open:
            Serial.print("UDP Open");
            break;
        case W5500::TCP_Listen:
            Serial.print("TCP Listen (Server)");
            break;
        case W5500::TCP_Connected:
            Serial.print("TCP Connected");
            break;
        case W5500::Temporary:
            Serial.print("Temporary");
            break;
    }
}
