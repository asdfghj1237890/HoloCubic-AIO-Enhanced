#ifndef AIO_STUB_WIRE_H
#define AIO_STUB_WIRE_H
#include "Arduino.h"
class TwoWire {
public:
    void begin(int = -1, int = -1, uint32_t = 100000) {}
    void beginTransmission(uint8_t) {}
    uint8_t endTransmission(bool = true) { return 0; }
    size_t requestFrom(uint8_t, size_t, bool = true) { return 0; }
    int available() { return 0; }
    int read() { return 0; }
    size_t write(uint8_t) { return 1; }
    size_t write(const uint8_t *, size_t n) { return n; }
    void setClock(uint32_t) {}
};
extern TwoWire Wire;
#endif
