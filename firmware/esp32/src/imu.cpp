#include "imu.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

static Adafruit_BNO055 bno(55, IMU_ADDR, &Wire);
static bool initialized = false;

bool imu_init() {
    Wire.begin(IMU_SDA, IMU_SCL);
    if (!bno.begin()) {
        return false;
    }
    // External crystal improves accuracy; required before entering any fusion mode.
    bno.setExtCrystalUse(true);
    // IMU mode: accel + gyro only, no magnetometer.
    bno.setMode(OPERATION_MODE_IMUPLUS);
    initialized = true;
    return true;
}

bool imu_read(float* ax, float* ay, float* az,
              float* gx, float* gy, float* gz) {
    if (!initialized) return false;
    imu::Vector<3> accel = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);
    imu::Vector<3> gyro  = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
    *ax = (float)accel.x();
    *ay = (float)accel.y();
    *az = (float)accel.z();
    *gx = (float)gyro.x();
    *gy = (float)gyro.y();
    *gz = (float)gyro.z();
    return true;
}
