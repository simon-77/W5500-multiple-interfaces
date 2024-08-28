#ifndef W5500_H
#define W5500_H

#include "SpiFrame.h"


/**
 * @brief W5500 Ethernet Controller
 * 
 * This class provides functions for managing the W5500 Ethernet controller.
 * It includes socket management, data transmission and reception, and
 * configuration of MAC, IP, and port settings.
 */
class W5500 {
public:
    //=============================
    // Type Definitions

    //-----------------------------
    // Socket
    enum SocketMode{
        TCP_Server,
        TCP_Client,
        UDP,
    };
    enum SocketStatus{
        Closed          = 0,
        UDP_Open        = 1,
        TCP_Listen      = 2, // TCP Server waiting for connection
        TCP_Connected   = 3, // TCP connection established (client or server)
        Temporary       = 4, // temporary states of W5500
    };
    
    //-----------------------------
    // IP, MAC, Port - Types
    using IP_t = uint8_t[4];    // e.g. IP_t ip = {192, 168, 0, 1};
    using MAC_t = uint8_t[6];
    using Port_t = uint16_t;

    //-----------------------------
    // Address Selection
    enum InterfaceAddress{
        GatewayIP,
        SubnetMask,
        SourceIP,
        SourceMAC,
    };
    enum SocketAddress{
        DestinationIP,
        DestinationMAC,
    };
    enum SocketPort{
        SourcePort,
        DestinationPort,
    };

    //=============================
    // Constructor
    W5500(SpiFrame &spiFrame);

    //=============================
    // Functions

    //-----------------------------
    // Initialization
    void init();

    //-----------------------------
    // Socket Managment
    bool socketOpen(uint8_t socket_n, SocketMode mode);
    void socketClose(uint8_t socket_n);
    void socketKeepOpen(uint8_t socket_n, SocketMode mode);

    SocketStatus socketStatus(uint8_t socket_n);
    bool socketConnected(uint8_t socket_n);

    //-----------------------------
    // Send & Receive data
    uint16_t sendAvailable(uint8_t socket_n);
    uint16_t receiveAvailable(uint8_t socket_n);

    uint16_t send(uint8_t socket_n, uint8_t *data, uint16_t len);
    uint16_t receive(uint8_t socket_n, uint8_t *data, uint16_t len);

    //-----------------------------
    // MAC, IP & Port configuration

    // Set Interface (common to all sockets)
    void setInterfaceNetwork(const IP_t source_ip, const IP_t subnet_mask, const IP_t gateway);
    void setInterfaceMAC(const MAC_t source_mac);
    
    // Set Socket Source (all socket modes) & Destination (TCP_Client & UDP only)
    void setSocketSource(uint8_t socket_n, Port_t source_port);
    void setSocketDest(uint8_t socket_n, IP_t dest_ip, Port_t dest_port);

    // Get Socket Ports
    Port_t getSocketPort(uint8_t socket_n, SocketPort select);
    
    // IP- & MAC-Address register access
    void regInterfaceAddress(InterfaceAddress select, bool write, uint8_t *data, uint8_t len, uint8_t offset = 0);
    void regSocketAddress(uint8_t socket_n, SocketAddress select, bool write, uint8_t *data, uint8_t len, uint8_t offset = 0);

    // W5500 RX- & TX-Buffer Size
    void setBufferSizeRx(uint8_t socket_n, uint8_t buff_size_kB);
    void setBufferSizeTx(uint8_t socket_n, uint8_t buff_size_kB);
    uint8_t getBufferSizeRx(uint8_t socket_n);
    uint8_t getBufferSizeTx(uint8_t socket_n);

    //-----------------------------
    // Status
    uint8_t phyStatus();
    uint8_t chipVersion();
    

    //=============================
    //=============================
private:
    //=============================
    // Type Definitions

    //-----------------------------
    // Register Offset Addresses
    enum CommonOffsetAddr{
        mode_register       = 0x0000,
        gateway_ip          = 0x0001,   // 0x0001 - 0x0004
        subnet_mask         = 0x0005,   // 0x0005 - 0x0008
        source_mac          = 0x0009,   // 0x0009 - 0x000E
        source_ip           = 0x000F,   // 0x000F - 0x0012
        unreachable_ip      = 0x0028,   // 0x0028 - 0x002B
        unreachable_port    = 0x002C,   // 0x002C - 0x002D
        phy_config          = 0x002E,
        versionr            = 0x0039,
    };
    enum SocketOffsetAddr{
        mode_register       = 0x0000,
        command_register    = 0x0001,
        status_register     = 0x0003,
        // MAC, IP, Port
        source_port         = 0x0004,   // 0x0004 - 0x0005
        destination_mac     = 0x0006,   // 0x0006 - 0x000B
        destination_ip      = 0x000C,   // 0x000C - 0x000F
        destination_port    = 0x0010,   // 0x0010 - 0x0011
        // Buffer sizes
        rxbuf_size          = 0x001E,
        txbuf_size          = 0x001F,
        // Buffer pointers
        tx_free_size        = 0x0020,   // 0x0020 - 0x0021
        tx_read_pointer     = 0x0022,   // 0x0022 - 0x0023
        tx_write_pointer    = 0x0024,   // 0x0024 - 0x0025
        rx_received_size    = 0x0026,   // 0x0026 - 0x0027
        rx_read_pointer     = 0x0028,   // 0x0028 - 0x0029
        rx_write_pointer    = 0x002A,   // 0x002A - 0x002B
    };
    //-----------------------------
    // Commands & Status - Values
    enum SocketCommandReg {
        OPEN                = 0x01,
        LISTEN              = 0x02,
        CONNECT             = 0x04,
        DISCONNECT          = 0x08,
        CLOSE               = 0x10,
        SEND                = 0x20,
        RECV                = 0x40,
    };
    enum SocketStatusReg {
        SOCK_CLOSED         = 0x00,
        SOCK_INIT           = 0x13,
        SOCK_LISTEN         = 0x14,
        SOCK_ESTABLISHED    = 0x17,
        SOCK_CLOSE_WAIT     = 0x1C,
        SOCK_UDP            = 0x22,
    };

    
    //=============================
    // Constants

    //-------------------
    // Register-Value Constants

    // Mode Register Bits - b5: 0=disable Wake-on-LAN, b4: 0=disable ping block, b3: 0=disable PPPoE, b1: 0=disable Force ARP
    const uint8_t common_mode_register = 0x00;
    // PHY Configuration Bits - b7: 1=no reset, b6: 1=configure PHY with PHYCFGR bits, b5-3: '111'=All capable, Auto-negotiation
    const uint8_t phy_config_register = 0xF8;
    // Socket Mode Bits - b7: 0=disable multicast (UDP), b6: 1=broadcast blocking (UDP), b5: 0=delayed ACK (TCP), b3: 0=disable unicast blocking (UDP)
    const uint8_t socket_mode_register_default = 0x40;

    //-------------------
    // Timeout Constants

    // Single timeout for various operations
    const float socket_timeout = 3.0; // seconds


    //=============================
    // Variables

    // Class reference to the SPI communication
    SpiFrame &spiFrame;


    //=============================
    // Functions

    //-----------------------------
    // Register Operations

    // Common Register Read/Write
    void wrCommonReg(CommonOffsetAddr offset, uint8_t data);
    uint8_t rdCommonReg(CommonOffsetAddr offset);

    // Socket Register Read/Write
    void wrSocketReg(uint8_t socket_n, SocketOffsetAddr offset, uint8_t data);
    void wrSocketReg16(uint8_t socket_n, SocketOffsetAddr offset, uint16_t data);
    uint8_t rdSocketReg(uint8_t socket_n, SocketOffsetAddr offset);
    uint16_t rdSocketReg16(uint8_t socket_n, SocketOffsetAddr offset);
    uint16_t rdSocketReg16_atomic(uint8_t socket_n, SocketOffsetAddr offset);

    // Socket Commands & Status
    void socketCommand(uint8_t socket_n, SocketCommandReg command);
    SocketStatusReg socketStatusReg(uint8_t socket_n);
    bool waitSocketStatus(uint8_t socket_n, SocketStatusReg status, float timeout);

    // Common & Scoket - Register Read/Write
    void commonReg(CommonOffsetAddr offset, bool write, uint8_t *data, uint16_t len);
    void socketReg(uint8_t socket_n, SocketOffsetAddr offset, bool write, uint8_t *data, uint16_t len);
    
    //-----------------------------
};

#endif // W5500_H