// Guard matches firmware driver/display.h. See common.h header.
#ifndef DISPLAY_H
#define DISPLAY_H
#include "lvgl.h"

class Display {
public:
    void init(uint8_t rotation, uint8_t backLight);
    void routine();
    void setBackLight(float);
};

#endif
