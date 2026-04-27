#ifndef AIO_STUB_TJPG_DECODER_H
#define AIO_STUB_TJPG_DECODER_H
#include "Arduino.h"

// Minimal TJpg_Decoder shim. screen_share + media_player only need the
// API surface to link; the harness never feeds real JPEG data so the
// callback is never invoked.

typedef int JRESULT;
#define JDR_OK     0
#define JDR_INTR   1
#define JDR_INP    2
#define JDR_FMT1   3
#define JDR_FMT2   4
#define JDR_FMT3   5
#define JDR_MEM1   6
#define JDR_MEM2   7

typedef bool (*SketchCallback)(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap);

class TJpg_Decoder {
public:
    void setJpgScale(uint8_t) {}
    void setCallback(SketchCallback) {}
    void setSwapBytes(bool) {}
    JRESULT drawJpg(int32_t, int32_t, const uint8_t *, uint32_t) { return JDR_OK; }
    JRESULT drawJpg(int32_t, int32_t, const String &) { return JDR_OK; }
    JRESULT drawSdJpg(int32_t, int32_t, const char *) { return JDR_OK; }
    JRESULT drawSdJpg(int32_t, int32_t, const String &) { return JDR_OK; }
    JRESULT getJpgSize(uint16_t *w, uint16_t *h, const uint8_t *, uint32_t) {
        if (w) *w = 0; if (h) *h = 0; return JDR_OK;
    }
    JRESULT getJpgSize(uint16_t *w, uint16_t *h, const String &) {
        if (w) *w = 0; if (h) *h = 0; return JDR_OK;
    }
};

extern TJpg_Decoder TJpgDec;

#endif
