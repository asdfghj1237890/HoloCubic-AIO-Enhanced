#include "display.h"
#include "aio_network.h"
#include "lv_port_indev.h"
#include "lv_demo_encoder.h"
#include "common.h"
#include <Arduino.h>

#define LV_HOR_RES_MAX_LEN 40

static uint8_t disp_buf[SCREEN_HOR_RES * LV_HOR_RES_MAX_LEN * 2];
static lv_display_t *display = NULL;

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft->setAddrWindow(area->x1, area->y1, w, h);
    tft->startWrite();
    tft->pushColors((uint16_t *)px_map, w * h, true);
    tft->endWrite();

    lv_display_flush_ready(disp);
}

void Display::init(uint8_t rotation, uint8_t backLight)
{
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(LCD_BL_PIN, 5000, 8);
#else
    ledcSetup(LCD_BL_PWM_CHANNEL, 5000, 8);
    ledcAttachPin(LCD_BL_PIN, LCD_BL_PWM_CHANNEL);
#endif

    lv_init();

    setBackLight(0.0); // 设置亮度 为了先不显示初始化时的"花屏"

    tft->begin(); /* TFT init */
    tft->fillScreen(TFT_BLACK);
    tft->writecommand(ST7789_DISPON); // Display on
    // tft->fillScreen(BLACK);

    // 尝试读取屏幕数据作为屏幕检测的依旧
    // uint8_t ret = tft->readcommand8(0x01, TFT_MADCTL);
    // Serial.printf("TFT read -> %u\r\n", ret);

    // 以下setRotation函数是经过更改的第4位兼容原版 高四位设置镜像
    // 正常方向需要设置为0 如果加上分光棱镜需要镜像改为4 如果是侧显示的需要设置为5
    tft->setRotation(rotation); /* mirror 修改反转，如果加上分光棱镜需要改为4镜像*/

    setBackLight(backLight / 100.0); // 设置亮度

    display = lv_display_create(SCREEN_HOR_RES, SCREEN_VER_RES);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, my_disp_flush);
    lv_display_set_buffers(display, disp_buf, NULL, sizeof(disp_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(display, tft);
}

void Display::routine()
{
    AIO_LVGL_OPERATE_LOCK(lv_timer_handler();)
}

void Display::setBackLight(float duty)
{
    duty = constrain(duty, 0, 1);
    duty = 1 - duty;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWrite(LCD_BL_PIN, (int)(duty * 255));
#else
    ledcWrite(LCD_BL_PWM_CHANNEL, (int)(duty * 255));
#endif
}
