#ifndef AIO_STUB_DRIVER_AMBIENT_H
#define AIO_STUB_DRIVER_AMBIENT_H
#include "Arduino.h"
#include "Wire.h"

#define ADDRESS_BH1750FVI 0x23
#define ONE_TIME_H_RESOLUTION_MODE 0x20
#define ONE_TIME_H_RESOLUTION_MODE2 0x21
#define ONE_TIME_L_RESOLUTION_MODE 0x23

class Ambient {
public:
    void init(int) {}
    unsigned int getLux() { return 100; }
};

#endif
