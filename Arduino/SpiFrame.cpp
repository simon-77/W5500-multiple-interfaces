#include "SpiFrame.h"

// Constructor
SpiFrame::SpiFrame(pin_size_t cs) : cs(cs) {}

/**
 * @brief Initialize the SPI communication
 */
void SpiFrame::init() {
    digitalWrite(cs, HIGH); // Deselect the SPI device
    
    SPI.begin();
    SPI.beginTransaction(w5500_spi_settings);
}


/**
 * @brief Transfer data to/from the W5500
 * @param frame Frame to transfer (read/write determines the behaviour of W5500)
 * @param data Data to write & read back (both will happen always)
 * @param len Length of the data
 */
void SpiFrame::transfer(Frame frame, uint8_t *data, uint16_t len) {
    digitalWrite(cs, LOW); // Select the SPI device
    // W5500: CS Setup Time = 5 ns
    SPI.transfer16(frame.offset_addr);
    SPI.transfer( (frame.socket_n<<2 | frame.bsb) << 3 | frame.rw << 2); // variable length data mode (VDM)
    SPI.transfer(data, len);
    // W5500: CS Hold Time = 5 ns
    digitalWrite(cs, HIGH); // Deselect the SPI device   
}

/**
 * @brief Wait for a specific value in a register, with a timeout
 * @param frame Frame to read from (only a single byte)
 * @param mask Mask to apply to the read value
 * @param value Value to compare with the masked read value (data & mask == value)
 * @param timeout_seconds Timeout in seconds
 * @return true if the value was found, false if the timeout was reached
 */
bool SpiFrame::wait_for_value(Frame frame, uint8_t mask, uint8_t value, float timeout_seconds) {
    const unsigned long start_time = millis();

    while ( (millis() - start_time) < static_cast<unsigned long>(timeout_seconds * 1000)) {
        uint8_t data;
        transfer(frame, &data, 1);
        if ( (data & mask) == value) {
            return true;
        }
        delay(1);
    }
    return false;
}

/**
 * @brief Sleep for a specific amount of time
 * @param seconds Time to sleep in seconds
 */
void SpiFrame::sleep(float seconds) {
    delay(static_cast<unsigned long>(seconds * 1000));
}