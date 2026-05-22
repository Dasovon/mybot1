#pragma once
#include <Arduino.h>

// GPIO assignments — Motor A = RIGHT, Motor B = LEFT
#define MOTOR_R_PWM   10   // PWMA — LEDC, 20 kHz
#define MOTOR_R_IN1   11   // AIN1
#define MOTOR_R_IN2   12   // AIN2
#define MOTOR_L_PWM   13   // PWMB — LEDC, 20 kHz
#define MOTOR_L_IN1   14   // BIN1
#define MOTOR_L_IN2   15   // BIN2

#define MOTOR_PWM_FREQ  20000   // 20 kHz — inaudible, no extra TB6612 heat
#define MOTOR_PWM_BITS  8       // 0–255 range
#define MOTOR_MAX_DUTY  255

// Maximum no-load wheel speed: ~190 rpm → rad/s
static constexpr float MOTOR_MAX_RAD_S = 19.9f;

void motors_init();

// duty: -1.0 (full reverse) to +1.0 (full forward) per wheel.
// Positive = forward. Values outside [-1, 1] are clamped.
void motors_set_duty(float right, float left);

// Coast both motors to a stop (direction pins LOW, PWM 0).
void motors_stop();
