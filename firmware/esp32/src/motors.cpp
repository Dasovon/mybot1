#include "motors.h"

#define MOTOR_R_CH  0
#define MOTOR_L_CH  1

void motors_init() {
    pinMode(MOTOR_R_IN1, OUTPUT);
    pinMode(MOTOR_R_IN2, OUTPUT);
    pinMode(MOTOR_L_IN1, OUTPUT);
    pinMode(MOTOR_L_IN2, OUTPUT);

    // arduino-esp32 2.x channel-based LEDC API
    ledcSetup(MOTOR_R_CH, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    ledcAttachPin(MOTOR_R_PWM, MOTOR_R_CH);
    ledcSetup(MOTOR_L_CH, MOTOR_PWM_FREQ, MOTOR_PWM_BITS);
    ledcAttachPin(MOTOR_L_PWM, MOTOR_L_CH);

    motors_stop();
}

static void set_channel(uint8_t ch, uint8_t in1, uint8_t in2, float duty) {
    duty = constrain(duty, -1.0f, 1.0f);
    uint32_t pwm = (uint32_t)(fabsf(duty) * MOTOR_MAX_DUTY);
    if (duty > 0.0f)      { digitalWrite(in1, HIGH); digitalWrite(in2, LOW);  }
    else if (duty < 0.0f) { digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); }
    else                  { digitalWrite(in1, LOW);  digitalWrite(in2, LOW);  }
    ledcWrite(ch, pwm);
}

void motors_set_duty(float right, float left) {
    set_channel(MOTOR_R_CH, MOTOR_R_IN1, MOTOR_R_IN2, right);
    set_channel(MOTOR_L_CH, MOTOR_L_IN1, MOTOR_L_IN2, left);
}

void motors_stop() {
    digitalWrite(MOTOR_R_IN1, LOW);
    digitalWrite(MOTOR_R_IN2, LOW);
    digitalWrite(MOTOR_L_IN1, LOW);
    digitalWrite(MOTOR_L_IN2, LOW);
    ledcWrite(MOTOR_R_CH, 0);
    ledcWrite(MOTOR_L_CH, 0);
}
