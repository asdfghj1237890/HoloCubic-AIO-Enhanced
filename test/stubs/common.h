// Guard intentionally matches firmware src/common.h. Some apps include
// "../../common.h" (relative) which lands on the firmware copy in
// addition to the stub. Aligning the guards ensures whichever lands
// second is short-circuited, preventing duplicate class definitions.
// The same trick is used for network.h, driver/*.h.
#ifndef COMMON_H
#define COMMON_H

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "driver/rgb_led.h"
#include "driver/flash_fs.h"
#include "driver/sd_card.h"
#include "driver/display.h"
#include "driver/ambient.h"
#include "driver/imu.h"
#include "network.h"

#define AIO_VERSION "2.5"
#define GET_SYS_MILLIS xTaskGetTickCount

extern IMU mpu;
extern SdCard tf;
extern Pixel rgb;
extern Network g_network;
extern FlashFS g_flashCfg;
extern Display screen;
extern Ambient ambLight;

boolean doDelayMillisTime(unsigned long interval,
                          unsigned long *previousMillis,
                          boolean state);

#define AMB_I2C_SDA 32
#define AMB_I2C_SCL 33
#define IMU_I2C_SDA 32
#define IMU_I2C_SCL 33
#define RGB_LED_PIN 27
#define SD_SCK 14
#define SD_MISO 26
#define SD_MOSI 13
#define SD_SS 15

#define SCREEN_HOR_RES 240
#define SCREEN_VER_RES 240
#define SCREEN_HEIGHT SCREEN_VER_RES
#define SCREEN_WIDTH SCREEN_HOR_RES

#define LCD_BL_PIN 5
#define LCD_BL_PWM_CHANNEL 0

#define TASK_RGB_PRIORITY 0
#define TASK_LVGL_PRIORITY 2

extern SemaphoreHandle_t lvgl_mutex;
#define AIO_LVGL_OPERATE_LOCK(CODE) { CODE; }

struct SysUtilConfig {
    String ssid_0;
    String password_0;
    String ssid_1;
    String password_1;
    String ssid_2;
    String password_2;
    String auto_start_app;
    uint8_t power_mode;
    uint8_t backLight;
    uint8_t rotation;
    uint8_t auto_calibration_mpu;
    uint8_t mpu_order;
};

#include <TFT_eSPI.h>
extern TFT_eSPI *tft;

#endif
