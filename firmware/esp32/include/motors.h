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

#define MOTOR_PWM_FREQ  1000    // 1 kHz — matches old articubot_one firmware; 20 kHz caused 10x speed loss
#define MOTOR_PWM_BITS  8       // 0–255 range
#define MOTOR_MAX_DUTY  255

// Static friction feedforward: baseline duty applied the moment target is nonzero,
// in the direction of motion. PID trims around this value. Keeps motors from stalling
// at low speeds and eliminates the integrator wind-up time to overcome friction.
// ~25% measured as the minimum duty to start motion on test chassis.
static constexpr float MOTOR_FEEDFORWARD = 0.25f;

// Measured no-load max on test chassis at 1 kHz PWM: ~0.218 m/s = 6.39 rad/s.
// Lower than 190 RPM spec (19.9 rad/s) due to chassis gearbox friction.
// Set slightly above measured max so PID can reach 100% duty at target.
static constexpr float MOTOR_MAX_RAD_S = 3.0f;

void motors_init();

// duty: -1.0 (full reverse) to +1.0 (full forward) per side.
// Positive = forward. Values outside [-1, 1] are clamped.
void motors_set_duty(float right, float left);

// Stop both sides (PWM = 0, DIR = LOW).
void motors_stop();
