#ifndef AIO_STUB_JPEGDEC_H
#define AIO_STUB_JPEGDEC_H
#include "Arduino.h"

// Minimal JPEGDEC shim for media_player. Decodes nothing on the host.
struct JPEGDRAW {
    int x, y, iWidth, iHeight, iBpp;
    uint16_t *pPixels;
};

typedef int (*JPEG_DRAW_CALLBACK)(JPEGDRAW *);

class JPEGDEC {
public:
    int openRAM(uint8_t *, int, JPEG_DRAW_CALLBACK) { return 0; }
    int openFLASH(uint8_t *, int, JPEG_DRAW_CALLBACK) { return 0; }
    int decode(int = 0, int = 0, int = 0) { return 0; }
    void close() {}
    int getWidth() { return 0; }
    int getHeight() { return 0; }
};

#endif
