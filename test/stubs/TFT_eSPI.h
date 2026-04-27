#ifndef AIO_STUB_TFT_ESPI_H
#define AIO_STUB_TFT_ESPI_H
#include "Arduino.h"

#define TFT_BLACK 0x0000
#define TFT_WHITE 0xFFFF
#define ST7789_DISPON 0x29

class TFT_eSPI {
public:
    TFT_eSPI() {}
    TFT_eSPI(int, int) {}
    void begin() {}
    void init() {}
    void setRotation(uint8_t) {}
    void fillScreen(uint16_t) {}
    void writecommand(uint8_t) {}
    void setAddrWindow(uint16_t, uint16_t, uint16_t, uint16_t) {}
    void startWrite() {}
    void endWrite() {}
    void pushColors(const void *, uint32_t, bool = true) {}
    void writePixels(const void *, uint32_t) {}
    int width() { return 240; }
    int height() { return 240; }
};

#endif
