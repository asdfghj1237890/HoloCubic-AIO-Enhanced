#ifndef AIO_STUB_WIFICLIENT_H
#define AIO_STUB_WIFICLIENT_H
#include "Arduino.h"

class WiFiClient {
public:
    int connect(const char *, uint16_t) { return 0; }
    int connect(uint32_t, uint16_t) { return 0; }
    void stop() {}
    int connected() { return 0; }
    int available() { return 0; }
    int read() { return -1; }
    int read(uint8_t *, size_t) { return 0; }
    size_t write(const uint8_t *, size_t n) { return n; }
    void flush() {}
    void setTimeout(uint32_t) {}
    operator bool() { return false; }
};

#endif
