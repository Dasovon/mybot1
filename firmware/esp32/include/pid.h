#pragma once

// KP=1.4: proportional response — provides fast initial drive.
// KI=3.0: integrator trims steady-state error around MOTOR_FEEDFORWARD baseline;
//   does not need to wake up the motors (feedforward handles that).
// KD=0.0: derivative off — encoder noise makes it more harmful than useful.
#define PID_KP_DEFAULT  1.4f
#define PID_KI_DEFAULT  3.0f
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
