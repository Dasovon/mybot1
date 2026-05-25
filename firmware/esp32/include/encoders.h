#pragma once
#include <Arduino.h>

// GPIO assignments — left encoder on GPIO 40/41 (EMI risk; 100 nF caps to GND required)
#define ENC_L_A  40
#define ENC_L_B  41
#define ENC_R_A  42
#define ENC_R_B  39

// Validated on floor: 3 runs averaged 1010 counts per wheel revolution.
// 2× quadrature (PCNT half-quad) through 45:1 gearbox. Use the floor-measured
// value — theoretical PPR derivation does not match exactly.
#define ENC_CPR  1010

// Uses ESP32-S3 PCNT hardware peripheral via ESP32Encoder library.
// Half-quadrature mode matches the 1010 CPR validated value.
// PCNT glitch filter provides EMI rejection on GPIO 40/41 (supplements hardware caps).
void encoders_init();

long encoders_get_left();
long encoders_get_right();
void encoders_reset();
