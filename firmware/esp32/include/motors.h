#pragma once
#include <Arduino.h>

// TB6612FNG — PWM + IN1 + IN2 per channel (articubot_one test hardware)
// Motor A = RIGHT side, Motor B = LEFT side
#define MOTOR_R_PWM   10   // PWMA — LEDC ch 0, 20 kHz, 8-bit
#define MOTOR_R_IN1   11   // AIN1
#define MOTOR_R_IN2   12   // AIN2
#define MOTOR_L_PWM   13   // PWMB — LEDC ch 1, 20 kHz, 8-bit
#define MOTOR_L_IN1   14   // BIN1
#define MOTOR_L_IN2   15   // BIN2

#define MOTOR_PWM_FREQ  20000   // 20 kHz — inaudible switching
#define MOTOR_PWM_BITS  8       // 0–255 range
#define MOTOR_MAX_DUTY  255

// Maximum no-load wheel speed: ~190 rpm → rad/s (same JGA25-371 motors)
static constexpr float MOTOR_MAX_RAD_S = 19.9f;

void motors_init();

// duty: -1.0 (full reverse) to +1.0 (full forward) per side.
// Positive = forward. Values outside [-1, 1] are clamped.
void motors_set_duty(float right, float left);

// Stop both sides (PWM = 0, DIR = LOW).
void motors_stop();
