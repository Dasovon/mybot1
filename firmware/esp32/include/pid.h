#pragma once

// Gains scaled from dev_ws articubot_one (KP=55, KI=15 in PWM units) to
// rad/s output architecture (divides by MOTOR_MAX_RAD_S before writing duty).
// KP = 55 * MOTOR_MAX_RAD_S / 255, KI = 15 * MOTOR_MAX_RAD_S / 255.
// MOTOR_MAX_RAD_S = 6.5 (measured on test chassis at 1 kHz PWM: ~0.218 m/s).
// KP = 55 * 6.5 / 255 = 1.4, KI = 15 * 6.5 / 255 = 0.38
#define PID_KP_DEFAULT  1.4f
#define PID_KI_DEFAULT  1.5f
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
