#ifndef AIO_UNIT_STUB_MPU6050_H
#define AIO_UNIT_STUB_MPU6050_H
#include "Arduino.h"
#include "I2Cdev.h"

// Test-controllable fake motion values. Tests set these via
// mpu_set_motion(...) before calling IMU::getAction(), then the stub
// MPU6050::getMotion6 below reads them out. IMU has a private MPU6050
// member so we route through globals rather than friending the class.
extern int16_t mpu_fake_ax, mpu_fake_ay, mpu_fake_az;
extern int16_t mpu_fake_gx, mpu_fake_gy, mpu_fake_gz;

inline void mpu_set_motion(int16_t ax, int16_t ay, int16_t az,
                           int16_t gx = 0, int16_t gy = 0, int16_t gz = 0) {
    mpu_fake_ax = ax; mpu_fake_ay = ay; mpu_fake_az = az;
    mpu_fake_gx = gx; mpu_fake_gy = gy; mpu_fake_gz = gz;
}

class MPU6050 {
public:
    MPU6050() {}
    MPU6050(uint8_t /*addr*/) {}
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
    void getMotion6(int16_t *ax, int16_t *ay, int16_t *az,
                    int16_t *gx, int16_t *gy, int16_t *gz) {
        if (ax) *ax = mpu_fake_ax;
        if (ay) *ay = mpu_fake_ay;
        if (az) *az = mpu_fake_az;
        if (gx) *gx = mpu_fake_gx;
        if (gy) *gy = mpu_fake_gy;
        if (gz) *gz = mpu_fake_gz;
    }
    void CalibrateAccel(uint8_t = 6) {}
    void CalibrateGyro(uint8_t = 6) {}
    void PrintActiveOffsets() {}
};

#endif
