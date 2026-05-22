#pragma once

// Default gains — tune on hardware after Phase 0 bench verification.
// Input and output are both in rad/s.
#define PID_KP_DEFAULT  2.0f
#define PID_KI_DEFAULT  0.5f
#define PID_KD_DEFAULT  0.02f

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
    // Clamps integrator to prevent windup: ±(MOTOR_MAX_RAD_S / ki) is sensible.
    static constexpr float INTEGRAL_LIMIT = 40.0f;
};
