#ifndef AIO_STUB_DRIVER_IMU_H
#define AIO_STUB_DRIVER_IMU_H
#include "Arduino.h"

#define ACTION_HISTORY_BUF_LEN 5

extern int32_t encoder_diff;
typedef int lv_indev_state_t;
extern lv_indev_state_t encoder_state;

extern const char *active_type_info[];

enum ACTIVE_TYPE
{
    TURN_RIGHT = 0,
    RETURN,
    TURN_LEFT,
    UP,
    DOWN,
    GO_FORWORD,
    SHAKE,
    UNKNOWN
};

enum MPU_DIR_TYPE
{
    NORMAL_DIR_TYPE = 0,
    X_DIR_TYPE = 0x01,
    Y_DIR_TYPE = 0x02,
    Z_DIR_TYPE = 0x04,
    XY_DIR_TYPE = 0x08
};

struct SysMpuConfig
{
    int16_t x_gyro_offset;
    int16_t y_gyro_offset;
    int16_t z_gyro_offset;
    int16_t x_accel_offset;
    int16_t y_accel_offset;
    int16_t z_accel_offset;
};

struct ImuAction
{
    volatile ACTIVE_TYPE active;
    boolean isValid;
    boolean long_time;
    int16_t v_ax, v_ay, v_az;
    int16_t v_gx, v_gy, v_gz;
};

class IMU {
public:
    ImuAction action_info;
    ACTIVE_TYPE act_info_history[ACTION_HISTORY_BUF_LEN];
    int act_info_history_ind;
    void init(uint8_t, uint8_t, SysMpuConfig *) {}
    void setOrder(uint8_t) {}
    bool Encoder_GetIsPush() { return false; }
    ImuAction *getAction() { return &action_info; }
    void getVirtureMotion6(ImuAction *) {}
};

#endif
