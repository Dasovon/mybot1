#pragma once

// P-only baseline (KP=0.25, KI=0) gave SS error −4.8% at 0.10 m/s (validated 2026-05-25).
// KI=0.5 introduced 2026-05-30 to drive SS error toward ~0%. Do not change KD until KI stable.
#define PID_KP_DEFAULT  0.25f
#define PID_KI_DEFAULT  0.5f
#define PID_KD_DEFAULT  0.0f

class PIDController {
public:
    PIDController(float kp, float ki, float kd, float dt);

    // Returns control output in the same units as setpoint/measured (rad/s).
    float compute(float setpoint, float measured);

    void reset();
    void set_gains(float kp, float ki, float kd);

private:
    float kp_, ki_, kd_, dt_;
    float integral_;
    float prev_error_;
    // Limits integrator contribution to ±2.0 rad/s * KI / MAX_RAD_S ≈ ±0.92 duty.
    // Feedforward handles friction; integrator only needs to trim the residual.
    static constexpr float INTEGRAL_LIMIT = 2.0f;
};
