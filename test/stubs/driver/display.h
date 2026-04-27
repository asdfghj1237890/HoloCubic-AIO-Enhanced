#ifndef AIO_STUB_DRIVER_DISPLAY_H
#define AIO_STUB_DRIVER_DISPLAY_H
#include "lvgl.h"

class Display {
public:
    void init(uint8_t rotation, uint8_t backLight);
    void routine();
    void setBackLight(float);
};

#endif
