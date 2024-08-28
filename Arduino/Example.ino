
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
W5500::IP_t eth1_ip = {172, 26, 123, 16};
W5500::IP_t eth1_subnet = {255, 255, 255, 0};
W5500::IP_t eth1_gateway = {172, 26 ,123, 1};

W5500::IP_t eth2_ip = {192, 168, 177, 18};
W5500::IP_t eth2_subnet = {255, 255, 255, 0};
W5500::IP_t eth2_gateway = {192, 168, 177, 1};


void SerialPrintlnIP(W5500::IP_t ip);
void SerialPrintlnMAC(W5500::MAC_t mac);


void setup()
{
    // Wait for serial connection before continuing
	  Serial.begin(115200);
    while(!Serial);
    Serial.println("Starting setup ...");
    
    // Configure Interface eth1
    eth1.init();
    eth1.setInterfaceMAC(eth1_mac);
    eth1.setInterfaceNetwork(eth1_ip, eth1_subnet, eth1_gateway);

    // Configure Interface eth2
    eth2.init();
    eth2.setInterfaceNetwork(eth2_ip, eth2_subnet, eth2_gateway);
    eth2.setInterfaceMAC(eth2_mac);



    // Read back the MAC and IP addresses and print them
    W5500::MAC_t mac;
    eth1.regInterfaceAddress(W5500::SourceMAC, false, mac, sizeof(mac));
    Serial.print("MAC Address: ");
    SerialPrintlnMAC(mac);
    
    W5500::IP_t ip_read;
    eth1.regInterfaceAddress(W5500::SourceIP, false, ip_read, sizeof(ip_read));
    Serial.print("IP Address: ");
    SerialPrintlnIP(ip_read);

    // Wait for the link to go "up" and print the status (bit 0: '1' = link up)
    delay(2000);
    Serial.print("PHY Status - eth1: ");
    Serial.print(eth1.phyStatus(), BIN);
    Serial.print(" ; eth2: ");
    Serial.println(eth2.phyStatus(), BIN);
    

    // Open a TCP server (socket 0) on eth1
    eth1.setSocketSource(0, 123);

    Serial.println(eth1.getSocketPort(0, W5500::SourcePort));
    if(eth1.socketOpen(0, W5500::TCP_Server)){
        Serial.println("Socket 0 opened on eth1");
    } else {
        Serial.println("Failed to open socket 0 on eth1");
    }

}

void loop()
{
    // In case the socket is closed, re-open it
    eth1.socketKeepOpen(0, W5500::TCP_Server);

    // Print the status of the socket
    switch(eth1.socketStatus(0)){
        case W5500::Closed:
            Serial.println("Socket 0 is closed");
            break;
        case W5500::UDP_Open:
            Serial.println("Socket 0 is open for UDP");
            break;
        case W5500::TCP_Listen:
            Serial.println("Socket 0 is listening for TCP connections");
            break;
        case W5500::TCP_Connected:
            Serial.println("Socket 0 is connected via TCP");
            break;
        case W5500::Temporary:
            Serial.println("Socket 0 is in a temporary state");
            break;
    }

    uint8_t buffer[1000];
    if (eth1.receiveAvailable(0) > 0) {
        int len = eth1.receive(0, buffer, sizeof(buffer));
        Serial.print("Received: ");
        Serial.write(buffer, len);
        Serial.println();
        uint8_t buffer2[] = "Hello from eth1";
        eth1.send(0, buffer2, 16);
        eth1.send(0, buffer, len);
    }

    delay(1000);
    
}



void SerialPrintlnIP(W5500::IP_t ip) {
    for (int i = 0; i < sizeof(W5500::IP_t); i++) {
        Serial.print(ip[i]);
        if (i < sizeof(W5500::IP_t) - 1) {
            Serial.print(".");
        }
    }
    Serial.println();
}

void SerialPrintlnMAC(W5500::MAC_t mac) {
    char hex_byte[3];
    for (int i = 0; i < sizeof(W5500::MAC_t); i++) {
        snprintf(hex_byte, sizeof(hex_byte), "%02X", mac[i]);
        Serial.print(hex_byte);
        if (i < sizeof(W5500::MAC_t) - 1) {
            Serial.print(":");
        }
    }
    Serial.println();
}
