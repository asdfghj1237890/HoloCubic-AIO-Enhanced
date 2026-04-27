// Guard matches firmware driver/rgb_led.h (RGB_H). See common.h header.
#ifndef RGB_H
#define RGB_H
#include "Arduino.h"
#include "FastLED.h"

#define RGB_LED_NUM 2
#define LED_MODE_RGB 0
#define LED_MODE_HSV 1

enum LED_RUN_MODE : unsigned char {
    RUN_MODE_TIMER = 0,
    RUN_MODE_TASK,
    RUN_MODE_NONE
};

class Pixel {
public:
    void init() {}
    Pixel &setRGB(int, int, int) { return *this; }
    Pixel &setHVS(uint8_t, uint8_t, uint8_t) { return *this; }
    Pixel &fill_rainbow(int, int, int, int, int, int) { return *this; }
    Pixel &setBrightness(float) { return *this; }
};

struct RgbConfig {
    uint8_t mode;
    uint8_t min_value_0, min_value_1, min_value_2;
    uint8_t max_value_0, max_value_1, max_value_2;
    int8_t step_0, step_1, step_2;
    uint16_t min_brightness, max_brightness;
    uint8_t brightness_step;
    int time;
};

struct RgbParam {
    uint8_t mode;
    uint8_t min_value_h, min_value_s, min_value_v;
    uint8_t max_value_h, max_value_s, max_value_v;
    int8_t step_h, step_s, step_v;
    uint16_t min_brightness, max_brightness;
    uint8_t brightness_step;
    int time;
};

inline bool set_rgb_and_run(RgbParam *, LED_RUN_MODE = RUN_MODE_TASK) { return true; }
inline void rgb_stop() {}

#endif
