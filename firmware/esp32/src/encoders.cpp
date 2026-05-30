#include "encoders.h"
#include <ESP32Encoder.h>

static ESP32Encoder enc_left;
static ESP32Encoder enc_right;

void encoders_init() {
    // Internal weak pull-ups reduce noise on open-drain encoder outputs
    ESP32Encoder::useInternalWeakPullResistors = UP;

    // Half-quadrature: counts both edges of channel A, uses B for direction.
    enc_left.attachHalfQuad(ENC_L_A, ENC_L_B);
    Serial0.printf("[ENC] left  attachHalfQuad A=GPIO%d B=GPIO%d  count after attach=%lld\n",
                   ENC_L_A, ENC_L_B, enc_left.getCount());

    enc_right.attachHalfQuad(ENC_R_A, ENC_R_B);
    Serial0.printf("[ENC] right attachHalfQuad A=GPIO%d B=GPIO%d  count after attach=%lld\n",
                   ENC_R_A, ENC_R_B, enc_right.getCount());

    enc_left.clearCount();
    enc_right.clearCount();
    Serial0.printf("[ENC] cleared — L=%lld R=%lld\n", enc_left.getCount(), enc_right.getCount());
}

long encoders_get_left()  { return (long)enc_left.getCount(); }
long encoders_get_right() { return (long)enc_right.getCount(); }

void encoders_reset() {
    enc_left.clearCount();
    enc_right.clearCount();
}
