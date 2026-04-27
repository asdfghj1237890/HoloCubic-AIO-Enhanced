#ifndef AIO_UNIT_STUB_ARDUINO_H
#define AIO_UNIT_STUB_ARDUINO_H

// Minimal Arduino.h stub for Track B (Unity unit tests). Only the
// surface that imu.cpp needs to compile + run on a desktop. Track A's
// stubs/Arduino.h is heavier (LVGL-aware String iterators, sleep
// helpers); we keep this lean so unit-test binaries link fast.

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef bool boolean;
typedef uint8_t byte;

#define F(s) (s)
#define PROGMEM

class HardwareSerial {
public:
    void begin(unsigned long) {}
    void print(const char *) {}
    void print(int) {}
    void println(const char *) {}
    void println(int) {}
    void printf(const char *, ...) {}
};
extern HardwareSerial Serial;

#endif
