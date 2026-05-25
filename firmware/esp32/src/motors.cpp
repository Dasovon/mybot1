#include "motors.h"

void motors_init() {
    pinMode(MOTOR_R_IN1, OUTPUT);
    pinMode(MOTOR_R_IN2, OUTPUT);
    pinMode(MOTOR_L_IN1, OUTPUT);
    pinMode(MOTOR_L_IN2, OUTPUT);

    // arduino-esp32 3.x pin-based LEDC API (channels assigned automatically)
    ledcAttach(MOTOR_R_PWM, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    ledcAttach(MOTOR_L_PWM, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);

    motors_stop();
}

static void set_channel(uint8_t pwm_pin, uint8_t in1, uint8_t in2, float duty) {
    duty = constrain(duty, -1.0f, 1.0f);
    uint32_t pwm = (uint32_t)(fabsf(duty) * MOTOR_MAX_DUTY);
    if (duty > 0.0f)      { digitalWrite(in1, HIGH); digitalWrite(in2, LOW);  }
    else if (duty < 0.0f) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); }
    else                  { digitalWrite(in1, LOW);  digitalWrite(in2, LOW);  }
    ledcWrite(pwm_pin, pwm);
}

void motors_set_duty(float right, float left) {
    set_channel(MOTOR_R_PWM, MOTOR_R_IN1, MOTOR_R_IN2, right);
    set_channel(MOTOR_L_PWM, MOTOR_L_IN1, MOTOR_L_IN2, left);
}

void motors_stop() {
    digitalWrite(MOTOR_R_IN1, LOW);
    digitalWrite(MOTOR_R_IN2, LOW);
    digitalWrite(MOTOR_L_IN1, LOW);
    digitalWrite(MOTOR_L_IN2, LOW);
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, 0);
}
