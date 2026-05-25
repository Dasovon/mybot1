#pragma once

// Default gains — scaled from dev_ws articubot_one validated values
// (KP=55, KI=15 in PWM units) to mybot1's rad/s output architecture
// (divides by MOTOR_MAX_RAD_S=19.9 before writing duty).
// KP = 55 * 19.9 / 255 ≈ 4.3, KI = 15 * 19.9 / 255 ≈ 1.2
#define PID_KP_DEFAULT  4.3f
#define PID_KI_DEFAULT  1.2f
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
    // Clamps integrator to prevent windup: ±(MOTOR_MAX_RAD_S / ki) is sensible.
    static constexpr float INTEGRAL_LIMIT = 40.0f;
};
