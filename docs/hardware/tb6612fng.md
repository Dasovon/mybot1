# TB6612FNG — Dual Motor Driver

## Role in This Project

The TB6612FNG drives both DC motors using PWM signals from the ESP32-S3. It handles the high-current switching that the ESP32 GPIO cannot handle directly.

---

## Key Specs

| Property | Value |
|---|---|
| Motor supply voltage (VM) | 2.5V – 13.5V |
| Logic supply voltage (VCC) | 2.7V – 5.5V |
| Output current (continuous) | 1.2A per channel |
| Output current (peak) | 3.2A per channel |
| PWM frequency | Up to 100 kHz |
| Channels | 2 (A and B) |
| Standby pin | Active LOW — must be pulled HIGH to enable |

---

## Wiring — Confirmed Pin Assignments (ESP32-S3 production stack)

**Breakout board:** Adafruit TB6612FNG (product #2448).
STBY → **not wired** — Adafruit breakout has onboard 10 kΩ pull-up (defaults HIGH = enabled).
Motor A = **RIGHT** | Motor B = **LEFT**

| TB6612 Pin | ESP32 GPIO | Function |
|---|---|---|
| VCC | 3V3 | Logic supply (3.3V — no level shifter needed) |
| VM | Battery V+ (via PD Hat VIN) | Motor power supply |
| GND | GND | Common ground |
| PWMA | GPIO 10 | Right motor speed (PWM, LEDC ch 0, 1 kHz 8-bit) |
| AIN1 | GPIO 11 | Right motor direction A |
| AIN2 | GPIO 12 | Right motor direction B |
| PWMB | GPIO 13 | Left motor speed (PWM, LEDC ch 1, 1 kHz 8-bit) |
| BIN1 | GPIO 14 | Left motor direction A |
| BIN2 | GPIO 15 | Left motor direction B |
| STBY | — | Not wired — onboard pull-up enabled |
| AO1, AO2 | Right motor terminals | Both wires on MOTORA pads (not the GND pad between sections) |
| BO1, BO2 | Left motor terminals | Both wires on MOTORB pads (not the GND pad between sections) |

> ⚠️ **Warning:** Max safe logic input on the ESP32 3.3V stack is **3.8V**. If the 12V VM wire ever bridges to any signal pin (AIN1/AIN2/BIN1/BIN2), the input gates will be destroyed instantly. Verify VM has no breadboard or wiring path to any signal pin before powering.

### Motor Direction Truth Table

| AIN1 / BIN1 | AIN2 / BIN2 | State |
|---|---|---|
| HIGH | LOW | Forward |
| LOW | HIGH | Reverse |
| LOW | LOW | Coast |
| HIGH | HIGH | Brake |

---

## PWM Configuration

- Use a PWM frequency between 10–20 kHz to reduce audible motor whine.
- PWM duty cycle maps linearly to motor speed.
- The PID controller outputs a target velocity in rad/s; the firmware maps this to a PWM duty cycle.

---

## Wiring Notes

- All grounds (ESP32, TB6612, battery, Pi) must share a common ground.
- Decouple VM with a 100µF electrolytic + 100nF ceramic capacitor close to the VM/GND pins.
- Keep motor wiring short and away from I2C/serial signal lines to minimize noise.

---

## Common Issues

| Symptom | Likely Cause |
|---|---|
| Motor twitching / erratic PWM | Missing or floating common ground |
| No motor movement | STBY pin not pulled HIGH |
| Motor runs one direction only | AIN1/AIN2 swapped or stuck |
| Overheating | Continuous current exceeds 1.2A; check load / add heatsink |
