#include "motors.h"

void motors_init() {
    pinMode(MOTOR_R_DIR, OUTPUT);
    pinMode(MOTOR_L_DIR, OUTPUT);

    // arduino-esp32 3.x pin-based LEDC API
    ledcAttach(MOTOR_R_PWM, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    ledcAttach(MOTOR_L_PWM, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);

    motors_stop();
}

static void set_channel(uint8_t pwm_pin, uint8_t dir_pin, float duty) {
    duty = constrain(duty, -1.0f, 1.0f);
    digitalWrite(dir_pin, duty >= 0.0f ? HIGH : LOW);
    ledcWrite(pwm_pin, (uint32_t)(fabsf(duty) * MOTOR_MAX_DUTY));
}

void motors_set_duty(float right, float left) {
    set_channel(MOTOR_R_PWM, MOTOR_R_DIR, right);
    set_channel(MOTOR_L_PWM, MOTOR_L_DIR, left);
}

void motors_stop() {
    digitalWrite(MOTOR_R_DIR, LOW);
    digitalWrite(MOTOR_L_DIR, LOW);
    ledcWrite(MOTOR_R_PWM, 0);
    ledcWrite(MOTOR_L_PWM, 0);
}
