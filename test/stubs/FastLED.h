#ifndef AIO_STUB_FASTLED_H
#define AIO_STUB_FASTLED_H
#include "Arduino.h"

struct CRGB {
    uint8_t r=0,g=0,b=0;
    CRGB() {}
    CRGB(uint8_t rr, uint8_t gg, uint8_t bb) : r(rr), g(gg), b(bb) {}
    CRGB(uint32_t v) : r((v>>16)&0xff), g((v>>8)&0xff), b(v&0xff) {}
    CRGB &operator=(uint32_t v) { r=(v>>16)&0xff; g=(v>>8)&0xff; b=v&0xff; return *this; }
};

struct CHSV { uint8_t h=0,s=0,v=0; CHSV() {} CHSV(uint8_t hh, uint8_t ss, uint8_t vv): h(hh), s(ss), v(vv) {} };

inline CRGB hsv2rgb_rainbow(const CHSV &) { return CRGB(); }

class FastLEDClass {
public:
    void show() {}
    void clear() {}
    void setBrightness(uint8_t) {}
    template <int LED_TYPE, int PIN, int ORDER = 0>
    FastLEDClass &addLeds(CRGB *, int) { return *this; }
};

extern FastLEDClass FastLED;

#define WS2812B 0
#define GRB 0

#endif
