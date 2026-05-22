#include "motors.h"

void motors_init() {
    pinMode(MOTOR_R_IN1, OUTPUT);
    pinMode(MOTOR_R_IN2, OUTPUT);
    pinMode(MOTOR_L_IN1, OUTPUT);
    pinMode(MOTOR_L_IN2, OUTPUT);

    // arduino-esp32 3.x pin-based LEDC API (no explicit channel selection)
    ledcAttach(MOTOR_R_PWM, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    ledcAttach(MOTOR_L_PWM, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);

    motors_stop();
}

void motors_set_duty(float right, float left) {
    // Right motor
    float r = constrain(right, -1.0f, 1.0f);
    int duty_r = (int)(fabsf(r) * MOTOR_MAX_DUTY);
    if (r >= 0.0f) {
        digitalWrite(MOTOR_R_IN1, HIGH);
        digitalWrite(MOTOR_R_IN2, LOW);
    } else {
        digitalWrite(MOTOR_R_IN1, LOW);
        digitalWrite(MOTOR_R_IN2, HIGH);
    }
    ledcWrite(MOTOR_R_PWM, duty_r);

    // Left motor
    float l = constrain(left, -1.0f, 1.0f);
    int duty_l = (int)(fabsf(l) * MOTOR_MAX_DUTY);
    if (l >= 0.0f) {
        digitalWrite(MOTOR_L_IN1, HIGH);
        digitalWrite(MOTOR_L_IN2, LOW);
    } else {
        digitalWrite(MOTOR_L_IN1, LOW);
        digitalWrite(MOTOR_L_IN2, HIGH);
    }
    ledcWrite(MOTOR_L_PWM, duty_l);
}

void motors_stop() {
    digitalWrite(MOTOR_R_IN1, LOW);
    digitalWrite(MOTOR_R_IN2, LOW);
    digitalWrite(MOTOR_L_IN1, LOW);
    digitalWrite(MOTOR_L_IN2, LOW);
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, 0);
}
