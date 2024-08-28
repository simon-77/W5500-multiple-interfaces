# W5500-multiple-interfaces
Ethernet library allowing to use multiple W5500 internet controllers.

Due to a lack of software libraries allowing to control multiple W5500 ethernet interfaces from one microcontroller, this object oriented library was created. Furthermore, the class can be easily adapted for different microcontrollers, allowing for seamless integration with different hardware platforms.

Features:
- Control multiple W5500 ethernet interfaces with one microcontroller.
- Shared SPI bus, only one dedicated chip-select line required per IC.
- Implemented TCP/IP stack allowing TCP (server & client) and UDP sockets.
- Allows modifying the TX & RX buffer sizes for each socket.

Missing Features:
- Control of various W5500 TCP/IP related registers not implemented (left at sensible default values).
- No interrupts (polling only)
- No Wake-on-LAN / PPPoE
- Unreachable IP/Port (from ICMP reply) can not be read
- No DHCP/DNS functionallity

Note: This documentation comment is a general overview of the class and its capabilities. For detailed information on the class methods, parameters, and usage examples, please refer to the code documentation.
