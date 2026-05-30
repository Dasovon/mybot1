#pragma once

// P-only baseline — matched to validated bare-bones serial test (Python Kp=20, FF=64).
// Translation: KP = Kp_py * MOTOR_MAX_RAD_S / 255 = 20 * 3.0 / 255 ≈ 0.235 → 0.25
// KI=0, KD=0: reintroduce only after P-only is stable under ROS.
#define PID_KP_DEFAULT  0.25f
#define PID_KI_DEFAULT  0.0f
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
