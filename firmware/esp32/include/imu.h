#pragma once
#include <Arduino.h>

#define IMU_SDA   8
#define IMU_SCL   9
#define IMU_ADDR  0x28

// Initializes Wire on GPIO 8/9 and the BNO055.
// Returns false if the sensor does not respond.
bool imu_init();

// Reads linear acceleration (m/s²) and angular velocity (rad/s).
// Magnetometer is disabled — unreliable on metal chassis.
// Returns false if the sensor is not initialized or read fails.
// Caller must hold g_i2c_mutex before calling this function.
bool imu_read(float* ax, float* ay, float* az,
              float* gx, float* gy, float* gz);
