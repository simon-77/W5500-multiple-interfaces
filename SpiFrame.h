#ifndef SPI_FRAME_H
#define SPI_FRAME_H

#include <Arduino.h>
#include <SPI.h>

// ARDUINO Implementation of `SpiFrame` class, used by the W5500 class

class SpiFrame {
public:
    enum BlockSelect {
        CommonReg = 0,  // socket_n must be 0 !!!
        SocketReg = 1,
        TxBuffer = 2,
        RxBuffer = 3
    };
    enum ReadWrite {
        Read = 0,
        Write = 1
    };
    struct Frame {
        uint16_t offset_addr;
        uint8_t socket_n;
        BlockSelect bsb;
        ReadWrite rw;
    };

    // Constructor
    SpiFrame(pin_size_t cs);
    void init();
    
    void transfer(Frame frame, uint8_t *data, uint16_t len);
    bool wait_for_value(Frame frame, uint8_t mask, uint8_t value, float timeout_seconds);
    void sleep(float seconds);

private:
    const SPISettings w5500_spi_settings = SPISettings(33e6, MSBFIRST, SPI_MODE0); // 33 MHz

    pin_size_t cs;
};

#endif // SPI_FRAME_H