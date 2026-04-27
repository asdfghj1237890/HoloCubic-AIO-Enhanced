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
    void fillRect(int32_t, int32_t, int32_t, int32_t, uint16_t) {}
    void drawRect(int32_t, int32_t, int32_t, int32_t, uint16_t) {}
    void writecommand(uint8_t) {}
    void setAddrWindow(uint16_t, uint16_t, uint16_t, uint16_t) {}
    void startWrite() {}
    void endWrite() {}
    void pushColors(const void *, uint32_t, bool = true) {}
    void pushImage(int32_t, int32_t, uint32_t, uint32_t, const uint16_t *) {}
    void pushImage(int32_t, int32_t, uint32_t, uint32_t, const uint16_t *, bool) {}
    void pushImage(int32_t, int32_t, uint32_t, uint32_t, const uint8_t *) {}
    void writePixels(const void *, uint32_t) {}
    void setTextColor(uint16_t) {}
    void setTextColor(uint16_t, uint16_t) {}
    void setTextSize(uint8_t) {}
    void setCursor(int16_t, int16_t) {}
    void setCursor(int16_t, int16_t, uint8_t) {}
    void setTextFont(uint8_t) {}
    void drawChar(int32_t, int32_t, uint16_t, uint16_t, uint16_t, uint8_t) {}
    void drawChar(char, int32_t, int32_t, uint8_t) {}
    void drawChar(uint16_t) {}
    void drawString(const char *, int32_t, int32_t) {}
    void drawString(const char *, int32_t, int32_t, uint8_t) {}
    void print(const char *) {}
    void print(int) {}
    void println(const char *) {}
    void println(int) {}
    static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
    int width() { return 240; }
    int height() { return 240; }
};

#endif
