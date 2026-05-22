#include "pid.h"
#include <algorithm>

PIDController::PIDController(float kp, float ki, float kd, float dt)
    : kp_(kp), ki_(ki), kd_(kd), dt_(dt), integral_(0.0f), prev_error_(0.0f) {}

float PIDController::compute(float setpoint, float measured) {
    float error = setpoint - measured;

    integral_ += error * dt_;
    integral_ = std::max(-INTEGRAL_LIMIT, std::min(INTEGRAL_LIMIT, integral_));

    float derivative = (error - prev_error_) / dt_;
    prev_error_ = error;

    return kp_ * error + ki_ * integral_ + kd_ * derivative;
}

void PIDController::reset() {
    integral_   = 0.0f;
    prev_error_ = 0.0f;
}

void PIDController::set_gains(float kp, float ki, float kd) {
    kp_ = kp;
    ki_ = ki;
    kd_ = kd;
    reset();
}
