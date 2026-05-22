#include "encoders.h"
#include <ESP32Encoder.h>

static ESP32Encoder enc_left;
static ESP32Encoder enc_right;

void encoders_init() {
    // Internal weak pull-ups reduce noise on open-drain encoder outputs
    ESP32Encoder::useInternalWeakPullResistors = UP;

    // Half-quadrature: counts both edges of channel A, uses B for direction.
    // Gives ~1010 counts/rev — matches the validated ENC_CPR constant.
    enc_left.attachHalfQuad(ENC_L_A, ENC_L_B);
    enc_right.attachHalfQuad(ENC_R_A, ENC_R_B);

    enc_left.clearCount();
    enc_right.clearCount();
}

long encoders_get_left()  { return (long)enc_left.getCount(); }
long encoders_get_right() { return (long)enc_right.getCount(); }

void encoders_reset() {
    enc_left.clearCount();
    enc_right.clearCount();
}
