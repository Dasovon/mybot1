#pragma once
#include <Arduino.h>

// Initializes Serial1 transport (non-blocking). Must be called in setup().
// The state machine (WAITING → CONNECTED → DISCONNECTED → WAITING) runs inside
// microros_spin() which must be called from loop() on every iteration.
void microros_init();

// Advances the micro-ROS state machine and spins the executor.
// Non-blocking: returns immediately if the agent is not available.
void microros_spin();

// Publish functions — safe to call any time; no-op if agent is not connected.
void microros_publish_odom(float x, float y, float theta,
                           float vel_linear, float vel_angular);
void microros_publish_imu(float ax, float ay, float az,
                          float gx, float gy, float gz);
void microros_publish_enc_diag(long left, long right);

// Latest cmd_vel received from Nav2 / teleop. Returns 0 if no command yet.
float    microros_cmd_linear();
float    microros_cmd_angular();
uint32_t microros_last_cmd_ms();
