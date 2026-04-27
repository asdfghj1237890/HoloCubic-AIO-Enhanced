#ifndef AIO_STUB_SPI_H
#define AIO_STUB_SPI_H
#include "Arduino.h"
class SPIClass {
public:
    void begin(int = -1, int = -1, int = -1, int = -1) {}
    void end() {}
    void setFrequency(uint32_t) {}
    void beginTransaction(int = 0) {}
    void endTransaction() {}
    uint8_t transfer(uint8_t b) { return b; }
};
extern SPIClass SPI;
#endif
