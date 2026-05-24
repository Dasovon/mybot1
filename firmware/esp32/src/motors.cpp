#include "motors.h"

#define MOTOR_R_CH  0
#define MOTOR_L_CH  1

void motors_init() {
    pinMode(MOTOR_R_DIR, OUTPUT);
    pinMode(MOTOR_L_DIR, OUTPUT);

    // arduino-esp32 2.x channel-based LEDC API
    ledcSetup(MOTOR_R_CH, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    ledcAttachPin(MOTOR_R_PWM, MOTOR_R_CH);
    ledcSetup(MOTOR_L_CH, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    ledcAttachPin(MOTOR_L_PWM, MOTOR_L_CH);

    motors_stop();
}

static void set_channel(uint8_t ch, uint8_t dir_pin, float duty) {
    duty = constrain(duty, -1.0f, 1.0f);
    digitalWrite(dir_pin, duty >= 0.0f ? HIGH : LOW);
    ledcWrite(ch, (uint32_t)(fabsf(duty) * MOTOR_MAX_DUTY));
}

void motors_set_duty(float right, float left) {
    set_channel(MOTOR_R_CH, MOTOR_R_DIR, right);
    set_channel(MOTOR_L_CH, MOTOR_L_DIR, left);
}

void motors_stop() {
    digitalWrite(MOTOR_R_DIR, LOW);
    digitalWrite(MOTOR_L_DIR, LOW);
    ledcWrite(MOTOR_R_CH, 0);
    ledcWrite(MOTOR_L_CH, 0);
}
