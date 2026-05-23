#pragma once
#include <Arduino.h>

// Cytron MDD10A — PWM + DIR per channel
// Ch1 = right side (front + rear motors in parallel)
// Ch2 = left side  (front + rear motors in parallel)
#define MOTOR_R_PWM   10   // LEDC ch 0, 20 kHz, 8-bit
#define MOTOR_R_DIR   11
#define MOTOR_L_PWM   12   // LEDC ch 1, 20 kHz, 8-bit
#define MOTOR_L_DIR   13

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
