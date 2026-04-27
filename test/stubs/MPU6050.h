#ifndef AIO_STUB_MPU6050_H
#define AIO_STUB_MPU6050_H
#include "Arduino.h"
#include "I2Cdev.h"

class MPU6050 {
public:
    MPU6050() {}
    void initialize() {}
    bool testConnection() { return true; }
    void setXGyroOffset(int16_t) {}
    void setYGyroOffset(int16_t) {}
    void setZGyroOffset(int16_t) {}
    void setXAccelOffset(int16_t) {}
    void setYAccelOffset(int16_t) {}
    void setZAccelOffset(int16_t) {}
    int16_t getXGyroOffset() { return 0; }
    int16_t getYGyroOffset() { return 0; }
    int16_t getZGyroOffset() { return 0; }
    int16_t getXAccelOffset() { return 0; }
    int16_t getYAccelOffset() { return 0; }
    int16_t getZAccelOffset() { return 0; }
    void getMotion6(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz) {
        if (ax) *ax=0; if (ay) *ay=0; if (az) *az=16384;
        if (gx) *gx=0; if (gy) *gy=0; if (gz) *gz=0;
    }
    int16_t getDeviceID() { return 0x68; }
    void resetGyroscopePath() {}
    void CalibrateAccel(uint8_t = 6) {}
    void CalibrateGyro(uint8_t = 6) {}
    void PrintActiveOffsets() {}
};

#endif
