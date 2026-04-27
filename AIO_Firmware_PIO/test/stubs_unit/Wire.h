#ifndef AIO_UNIT_STUB_WIRE_H
#define AIO_UNIT_STUB_WIRE_H
#include "Arduino.h"

class TwoWire {
public:
    void begin(int, int) {}
    void setClock(uint32_t) {}
};
extern TwoWire Wire;

#endif
