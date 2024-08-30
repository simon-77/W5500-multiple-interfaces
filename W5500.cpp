#include "W5500.h"

#include <algorithm>

/**
 * @brief Constructor
 * @param spiFrame a "SpiFrame" object enabling communication with the W5500
 */
W5500::W5500(SpiFrame &spiFrame) : spiFrame(spiFrame) {}

/**
 * @brief Initialize the SPI interface & the W5500
 */
void W5500::init() {
    spiFrame.init();

    // Reset the W5500
    wrCommonReg(CommonOffsetAddr::common_mode_register, 0x80);
    spiFrame.sleep(0.001);
    wrCommonReg(CommonOffsetAddr::common_mode_register, common_mode_register_value);

    // Reset & Configure PHY (auto-negotiation)
    wrCommonReg(CommonOffsetAddr::phy_config, phy_config_register_value & 0x78);
    spiFrame.sleep(0.001);
    wrCommonReg(CommonOffsetAddr::phy_config, phy_config_register_value);
    spiFrame.sleep(0.001);
}

//=======================================================
// Socket Management (Open, Close, Maintain)
//=======================================================

/**
 * @brief Open a socket - IP & Port configuration is required before
 * @param socket_n Socket number
 * @param mode Mode of the socket
 * @return true if the socket was opened successfully, false if failed (e.g. the PHY link is down)
 * In case of TCP_Client, only return true if the TCP connection to the server was successful.
 * There are timeouts for each step, the function will return eventually.
 */
bool W5500::socketOpen(uint8_t socket_n, SocketMode mode) {
    if ( (rdCommonReg(CommonOffsetAddr::phy_config) & 0x01) == 0) {
        // PHY link is down
        return false;
    }

    // variables depending on the mode
    uint8_t mode_value = socket_mode_register_default;
    SocketCommandReg second_command;
    SocketStatusReg expected_state;
    switch (mode) {
        case TCP_Server:
            mode_value |= 0x01;
            second_command = LISTEN;
            expected_state = SOCK_LISTEN;
            break;
        case TCP_Client:
            mode_value |= 0x01;
            second_command = CONNECT;
            expected_state = SOCK_ESTABLISHED;
            break;
        case UDP:
            mode_value |= 0x02;
            break;
    }

    // ensure the socket is closed
    socketClose(socket_n);

    // set the mode & open (initialise) the socket
    wrSocketReg(socket_n, SocketOffsetAddr::socket_mode_register, mode_value);
    socketCommand(socket_n, OPEN);
    
    switch(mode) {
        // UDP Connection
        case UDP:
            if (waitSocketStatus(socket_n, SOCK_UDP, socket_timeout)) {
                return true;  // success
            }
            break;

        // TCP Connection
        case TCP_Server:
        case TCP_Client:
            // TCP Connection
            if(waitSocketStatus(socket_n, SOCK_INIT, socket_timeout)) {
                // socket is now in SOCK_INIT state
                socketCommand(socket_n, second_command);
                if(waitSocketStatus(socket_n, expected_state, socket_timeout)) {
                    return true;  // success
                }
            }
            break;
    }
    // timeout -> close the socket
    socketClose(socket_n);
    return false; // failure
}

/**
 * @brief Cloase a socket
 * @param socket_n Socket number
 * Try to use TCP-disconnect for TCP connection
 */
void W5500::socketClose(uint8_t socket_n) {
    switch(socketStatusReg(socket_n)) {
        case SOCK_CLOSED:
            return;
        case SOCK_ESTABLISHED:
        case SOCK_CLOSE_WAIT:
            //TCP connection
            socketCommand(socket_n, DISCONNECT);
            if (waitSocketStatus(socket_n, SOCK_CLOSED, socket_timeout)) {
                // socket is closed now
                return;
            }
    }
    socketCommand(socket_n, CLOSE);
}

/**
 * @brief Keep a socket open (re-open if socket was closed)
 * @param socket_n Socket number
 * @param mode Mode of the socket
 * The socket will only be re-opened if it was closed by some other means.
 * To change the socket mode, manually close it first.
 */
void W5500::socketKeepOpen(uint8_t socket_n, SocketMode mode) {
    switch(socketStatusReg(socket_n)) {
        case SOCK_CLOSED:
        case SOCK_INIT:
            socketOpen(socket_n, mode);
            break;
        case SOCK_LISTEN:       // TCP Server waiting for connection
        case SOCK_ESTABLISHED:  // TCP connection successful
            // do nothing
            break;
        case SOCK_CLOSE_WAIT:   // half-closing status -> TCP disconnection process
            socketCommand(socket_n, DISCONNECT);
            break;
        case SOCK_UDP:          // UDP connection successful
            // do nothing
            break;
        default:
            // there  are multiple temporary states -> simply wait
            break;
    }
}


/**
 * @brief Get the status of a socket & perform basic maintenance (e.g. close a half-closed socket)
 * @param socket_n Socket number
 * @return Status of the socket
 */
W5500::SocketStatus W5500::socketStatus(uint8_t socket_n) {
    switch(socketStatusReg(socket_n)) {
        //--------------------
        case SOCK_CLOSED:
            return Closed;
        //--------------------
        case SOCK_INIT:     // socket left in init state -> close
            socketCommand(socket_n, CLOSE);
            return Closed;
        //--------------------
        case SOCK_LISTEN:   // TCP Server waiting for connection
            return TCP_Listen;
        //--------------------
        case SOCK_ESTABLISHED:  // TCP connected (client or server)
            return TCP_Connected;
        //--------------------
        case SOCK_CLOSE_WAIT:   // half-closing status -> TCP disconnection process
            socketCommand(socket_n, DISCONNECT);
            return Closed;
        //--------------------
        case SOCK_UDP:    // UDP connection successful
            return UDP_Open;
        //--------------------
        default:
            return Temporary;
    }
}

/**
 * @brief Check if a socket is connected & perform basic maintenance (e.g. close a half-closed socket)
 * @param socket_n Socket number
 * @return true if the socket is connected (TCP or UDP), false otherwise (e.g. TCP_listen)
 */
bool W5500::socketConnected(uint8_t socket_n) {
    switch(socketStatus(socket_n)) {
        case TCP_Connected:
        case UDP_Open:
            return true;
        default:
            return false;
    }
}


//=======================================================
// Send & Receive data
//=======================================================

/**
 * @brief Get the number of bytes available (in TX Buffer) for sending
 * @param socket_n Socket number
 * @return Number of bytes available for sending, 0 if the socket is not connected
 */
uint16_t W5500::sendAvailable(uint8_t socket_n) {
    if (socketConnected(socket_n)) {
        return rdSocketReg16_atomic(socket_n, SocketOffsetAddr::tx_free_size);
    } else { // socket is not connected
        return 0;
    }
}

/**
 * @brief Get the number of bytes received (in RX Buffer) for reading
 * @param socket_n Socket number
 * @return Number of bytes available for reading, 0 if the socket is not connected
 */
uint16_t W5500::receiveAvailable(uint8_t socket_n) {
    if (socketConnected(socket_n)) {
        return rdSocketReg16_atomic(socket_n, SocketOffsetAddr::rx_received_size);
    } else { // socket is not connected
        return 0;
    }
}

/**
 * @brief Send data to socket.
 * @param socket_n Socket number
 * @param data Pointer to the sending data (will be overwritten with unimportant data)
 * @param len length of the data
 * @return actuall number of bytes sent
 * The SEND command will be issued, sending the data to the destination.
 */
uint16_t W5500::send(uint8_t socket_n, uint8_t *data, uint16_t len) {
    len = std::min(len, sendAvailable(socket_n));
    // no space available in buffer
    if (len == 0) {
        return 0;
    }

    //=== Write Operation
    // 1. Read starting address
    const uint16_t write_pointer = rdSocketReg16(socket_n, SocketOffsetAddr::tx_write_pointer);
    // 2. Write data to the buffer
    spiFrame.transfer(SpiFrame::Frame{write_pointer, socket_n, SpiFrame::TxBuffer, SpiFrame::Write}, data, len);
    // 3. Update the write pointer
    wrSocketReg16(socket_n, SocketOffsetAddr::tx_write_pointer, write_pointer + len);
    // 4. Send the data
    socketCommand(socket_n, SEND);

    return len;
}

/**
 * @brief Receive data from socket
 * @param socket_n Socket number
 * @param data Pointer for the received data
 * @param len maximum length of the receiving data
 * @param udpIgnoreHeader only applicable for UDP, if true only the payload will be received
 * @return actuall number of bytes received
 * The RECV command will be issued, updating the read pointer of W5500.
 * In UDP mode, the W5500 puts 8-byte Packet-Info before the Data packet.
 * <https://docs.wiznet.io/Product/iEthernet/W5500/Application/udp>
 * Put udpIgnoreHeader to true for only receiving the payload.
 */
uint16_t W5500::receive(uint8_t socket_n, uint8_t *data, uint16_t len, bool udpIgnoreHeader) {
    len = std::min(len, receiveAvailable(socket_n));
    // nothing to receive
    if (len == 0) {
        return 0;
    }

    //=== Read Operation
    // 1. Read starting address
    uint16_t read_pointer = rdSocketReg16(socket_n, SocketOffsetAddr::rx_read_pointer);
    //--- UDP-Header information in the first 8 bytes
    if (udpIgnoreHeader) {
        uint8_t udp_header[8]; // 4-byte destination IP, 2-byte destination port, 2-byte length of data packet
        if (len < 8) {
            return 0; // not enough space for the header
        }
        spiFrame.transfer(SpiFrame::Frame{read_pointer, socket_n, SpiFrame::RxBuffer, SpiFrame::Read}, udp_header, 8);
        len = std::min(len-8, ((uint16_t)udp_header[6]) << 8 | udp_header[7]); // read only one UDP data packet
        read_pointer += 8; // increase past
    }
    // 2. Read data from the buffer
    spiFrame.transfer(SpiFrame::Frame{read_pointer, socket_n, SpiFrame::RxBuffer, SpiFrame::Read}, data, len);
    // 3. Update the read pointer
    wrSocketReg16(socket_n, SocketOffsetAddr::rx_read_pointer, read_pointer + len);
    // 4. Notify the updated read pointer to W5500
    socketCommand(socket_n, RECV);

    return len;
}


//=======================================================
// IP & Port configuration
//=======================================================

//=============================
// Set Interface (common to all sockets)

void W5500::setInterfaceNetwork(const IP_t source_ip, const IP_t subnet_mask, const IP_t gateway) {
    // copy IP addresses to a buffer - regInterfaceAddress will overwrite it
    IP_t ip_buffer;
    memcpy(ip_buffer, source_ip, sizeof(IP_t));
    regInterfaceAddress(SourceIP, true, ip_buffer, sizeof(IP_t));
    memcpy(ip_buffer, subnet_mask, sizeof(IP_t));
    regInterfaceAddress(SubnetMask, true, ip_buffer, sizeof(IP_t));
    memcpy(ip_buffer, gateway, sizeof(IP_t));
    regInterfaceAddress(GatewayIP, true, ip_buffer, sizeof(IP_t));
    
}

void W5500::setInterfaceMAC(const MAC_t source_mac) {
    // copy MAC address to a buffer - regInterfaceAddress will overwrite it
    MAC_t mac_buffer;
    memcpy(mac_buffer, source_mac, sizeof(MAC_t));
    regInterfaceAddress(SourceMAC, true, mac_buffer, sizeof(MAC_t));
}

//=============================
// Set Socket Source & Destination

/**
 * @brief Set the source port of a socket (mandatory before opening any socket)
 * @param socket_n socket number
 * @param source_port source port number
 */
void W5500::setSocketSource(uint8_t socket_n, Port_t source_port) {
    wrSocketReg16(socket_n, SocketOffsetAddr::source_port, source_port);
}

/**
 * @brief Set the destination IP & Port of a socket (TCP_Client & UDP only)
 * @param socket_n socket number
 * @param dest_ip destination IP address
 * @param dest_port destination port number
 */
void W5500::setSocketDest(uint8_t socket_n, const IP_t dest_ip, Port_t dest_port) {
    // copy IP address to a buffer - regSocketAddress will overwrite it
    IP_t ip_buffer;
    memcpy(ip_buffer, dest_ip, sizeof(IP_t));
    regSocketAddress(socket_n, DestinationIP, true, ip_buffer, sizeof(IP_t));
    wrSocketReg16(socket_n, SocketOffsetAddr::destination_port, dest_port);
}

//=============================
// Get Socket Ports

/**
 * @brief Get the source/destination port of a socket
 * @param socket_n socket number
 * @return source/destination port number
 */
W5500::Port_t W5500::getSocketPort(uint8_t socket_n, SocketPort select) {
    switch(select) {
        case SourcePort:
            return rdSocketReg16(socket_n, SocketOffsetAddr::source_port);
        case DestinationPort:
            return rdSocketReg16(socket_n, SocketOffsetAddr::destination_port);
    }
    
}

//=============================
// IP- & MAC-Address register access

/**
 * @brief Register access (read & write) for INTERFACE IP- & MAC-addresses
 * @param select Address to select (GatewayIP, SubnetMask, SourceIP, SourceMAC)
 * @param write true for write, false for read
 * @param data pointer for reading or writing the data (to *data will always be written)
 * @param len length of the data (should be 4 for IP-addresses, 6 for MAC-addresses)
 * @param offset offset of the register (default: 0)
 * The SPI transfer will always write to the *data pointer. Even when setting a new address by setting write to true.
 */
void W5500::regInterfaceAddress(InterfaceAddress select, bool write, uint8_t *data, uint8_t len, uint8_t offset) {
    uint16_t socket_addr;
    int max_len;
    switch(select) {
        case GatewayIP:
            socket_addr = CommonOffsetAddr::gateway_ip;
            max_len = sizeof(IP_t);
            break;
        case SubnetMask:
            socket_addr = CommonOffsetAddr::subnet_mask;
            max_len = sizeof(IP_t);
            break;
        case SourceIP:
            socket_addr = CommonOffsetAddr::source_ip;
            max_len = sizeof(IP_t);
            break;
        case SourceMAC:
            socket_addr = CommonOffsetAddr::source_mac;
            max_len = sizeof(MAC_t);
            break;
        default:
            return;
    }
    max_len -= offset; // adjust for offset
    if (max_len <= 0) return; // offset too large
    len = std::min(len, (uint8_t)(max_len));
    commonReg((CommonOffsetAddr)(socket_addr + offset), write, data, len);
}

/**
 * @brief Register access (read & write) for SOCKET IP- & MAC-addresses
 * @param socket_n Socket number
 * @param select Address to select (DestinationIP, DestinationMAC)
 * @param write true for write, false for read
 * @param data pointer for reading or writing the data (to *data will always be written)
 * @param len length of the data (should be 4 for IP-addresses, 6 for MAC-addresses)
 * @param offset offset of the register (default: 0)
 * The SPI transfer will always write to the *data pointer. Even when setting a new address by setting write to true.
 */
void W5500::regSocketAddress(uint8_t socket_n, SocketAddress select, bool write, uint8_t *data, uint8_t len, uint8_t offset) {
    uint16_t socket_addr;
    int max_len;
    switch(select) {
        case DestinationIP:
            socket_addr = SocketOffsetAddr::destination_ip;
            max_len = sizeof(IP_t);
            break;
        case DestinationMAC:
            socket_addr = SocketOffsetAddr::destination_mac;
            max_len = sizeof(MAC_t);
            break;
        default:
            return;
    }
    max_len -= offset; // adjust for offset
    if (max_len <= 0) return; // offset too large
    len = std::min(len, (uint8_t)(max_len));
    socketReg(socket_n, (SocketOffsetAddr)(socket_addr + offset), write, data, len);
}

//=============================
// W5500 RX- & TX-Buffer Size

/**
 * @brief Set the size of the TX buffer of a socket
 * @param socket_n Socket number
 * @param size Size of the TX buffer in kB (allowed: 0, 1, 2, 4, 8, 16)
 * The sum of all TX buffer sizes must not exceed 16kB.
 */
void W5500::setBufferSizeRx(uint8_t socket_n, uint8_t buff_size_kB) {
    wrSocketReg(socket_n, SocketOffsetAddr::rxbuf_size, buff_size_kB);
}

/**
 * @brief Set the size of the RX buffer of a socket
 * @param socket_n Socket number
 * @param size Size of the RX buffer in kB (allowed: 0, 1, 2, 4, 8, 16)
 * The sum of all RX buffer sizes must not exceed 16kB.
 */
void W5500::setBufferSizeTx(uint8_t socket_n, uint8_t buff_size_kB) {
    wrSocketReg(socket_n, SocketOffsetAddr::txbuf_size, buff_size_kB);
}

/**
 * @brief Get the size of the RX buffer of a socket
 * @param socket_n Socket number
 * @return Size of the RX buffer in kB
 */
uint8_t W5500::getBufferSizeRx(uint8_t socket_n) {
    return rdSocketReg(socket_n, SocketOffsetAddr::rxbuf_size);
}

/**
 * @brief Get the size of the TX buffer of a socket
 * @param socket_n Socket number
 * @return Size of the TX buffer in kB
 */
uint8_t W5500::getBufferSizeTx(uint8_t socket_n) {
    return rdSocketReg(socket_n, SocketOffsetAddr::txbuf_size);
}


//=======================================================
// Status
//=======================================================

/**
 * @brief Get the PHY status bits
 * @return PHY status bits:
 * - bit 0: Link status (1 = link up, 0 = link down)
 * - bit 1: Speed       (1 = 100Mbps, 0 = 10Mbps)
 * - bit 2: Duplex      (1 = full duplex, 0 = half duplex)
 * - bit 3-7: always '0'
 */
uint8_t W5500::phyStatus() {
    return rdCommonReg(CommonOffsetAddr::phy_config) & 0x07;
}

/**
 * @brief Get the W5500 chip version
 * @return W5500 chip version register value (expected: 0x04)
 */
uint8_t W5500::chipVersion() {
    return rdCommonReg(CommonOffsetAddr::versionr);
}


//=======================================================
// Register Operations
//=======================================================


//=============================
// Common Register Read/Write

// write a single common register
void W5500::wrCommonReg(CommonOffsetAddr offset, uint8_t data) {
    commonReg(offset, true, &data, 1);
}

// read a single common register
uint8_t W5500::rdCommonReg(CommonOffsetAddr offset) {
    uint8_t data;
    commonReg(offset, false, &data, 1);
    return data;
}


//=============================
// Socket Register Read/Write

// write a single socket register
void W5500::wrSocketReg(uint8_t socket_n, SocketOffsetAddr offset, uint8_t data) {
    socketReg(socket_n, offset, true, &data, 1);
}

// write a 16-bit value to a socket register
void W5500::wrSocketReg16(uint8_t socket_n, SocketOffsetAddr offset, uint16_t data) {
    uint8_t data_8[2] = {data >> 8, data & 0xFF};
    socketReg(socket_n, offset, true, data_8, 2);
}

// read a single socket register
uint8_t W5500::rdSocketReg(uint8_t socket_n, SocketOffsetAddr offset) {
    uint8_t data;
    socketReg(socket_n, offset, false, &data, 1);
    return data;
}

// read a 16-bit value from a socket register
uint16_t W5500::rdSocketReg16(uint8_t socket_n, SocketOffsetAddr offset) {
    uint8_t data[2];
    socketReg(socket_n, offset, false, data, 2);
    return (data[0] << 8) | data[1];
}

// emulate atomic 16-bit read by checking if the value is stable (recommended by datasheet)
uint16_t W5500::rdSocketReg16_atomic(uint8_t socket_n, SocketOffsetAddr offset) {
    const uint8_t max_tries = 20;
    uint16_t last_value = 0;

    for (uint8_t tries = 0; tries < max_tries; tries++) {
        uint16_t value = rdSocketReg16(socket_n, offset);
        if ( (tries >= 1) && (value == last_value)) {
            return value;
        }
        last_value = value;
    }
    // failed to get a stable value
    return 0;
}


//=============================
// Socket Commands & Status

// send command to a socket
void W5500::socketCommand(uint8_t socket_n, SocketCommandReg command) {
    wrSocketReg(socket_n, SocketOffsetAddr::command_register, command);
}

// get the status of a socket
W5500::SocketStatusReg W5500::socketStatusReg(uint8_t socket_n) {
    return (SocketStatusReg)rdSocketReg(socket_n, SocketOffsetAddr::status_register);
}

/**
 * @brief Wait for a socket to reach a certain status or timeout
 * @param socket_n Socket number
 * @param status Status to wait for
 * @param timeout Timeout in seconds
 * @return true if the status was reached, false if the timeout was reached
 */
bool W5500::waitSocketStatus(uint8_t socket_n, SocketStatusReg status, float timeout) {
    const SpiFrame::Frame frame = {SocketOffsetAddr::status_register, socket_n, SpiFrame::SocketReg, SpiFrame::Read};
    return spiFrame.wait_for_value(frame, 0xFF, (uint8_t)status, timeout);
}


//=============================
// Common & Scoket - Register Read/Write

// read & write multiple common register (SPI will always write & read back the *data buffer)
void W5500::commonReg(CommonOffsetAddr offset, bool write, uint8_t *data, uint16_t len) {
    const SpiFrame::Frame frame = {offset, 0, SpiFrame::CommonReg, write ? SpiFrame::Write : SpiFrame::Read};
    spiFrame.transfer(frame, data, len);
}

// read & write multiple socket register (SPI will always write & read back the *data buffer)
void W5500::socketReg(uint8_t socket_n, SocketOffsetAddr offset, bool write, uint8_t *data, uint16_t len) {
    const SpiFrame::Frame frame = {offset, socket_n, SpiFrame::SocketReg, write ? SpiFrame::Write : SpiFrame::Read};
    spiFrame.transfer(frame, data, len);
}