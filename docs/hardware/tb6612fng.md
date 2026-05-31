# TB6612FNG — Dual Motor Driver

## Role in This Project

The TB6612FNG drives both DC motors using PWM signals from the ESP32-S3. It handles the high-current switching that the ESP32 GPIO cannot handle directly.

---

## Key Specs

| Property | Value |
|---|---|
| Motor supply voltage (VM) | 4.5V – 13.5V (DC motors); chip absolute min 2.5V |
| Logic supply voltage (VCC) | 2.7V – 5.5V |
| Output current (continuous) | 1.2A per channel |
| Output current (peak) | 3A per channel (~20 ms) |
| Kickback protection | Built-in kickback diodes on all motor outputs |
| PWM frequency | Up to 100 kHz |
| Channels | 2 (A and B) |
| Standby pin | Active LOW — must be pulled HIGH to enable |

---

## Breakout Pinout

**Breakout board:** Adafruit TB6612FNG (product #2448). STBY has onboard 10 kΩ pull-up (defaults HIGH = enabled — do not wire STBY).

Motor A = **RIGHT** | Motor B = **LEFT**

| Pin | Function |
|---|---|
| VCC | Logic supply (2.7–5.5V) |
| VM | Motor supply (2.5–13.5V) — connect to battery voltage |
| GND | Ground |
| PWMA | Right motor speed (PWM input) |
| AIN1, AIN2 | Right motor direction |
| AO1, AO2 | Right motor output — connect to right motor terminals |
| PWMB | Left motor speed (PWM input) |
| BIN1, BIN2 | Left motor direction |
| BO1, BO2 | Left motor output — connect to left motor terminals |
| STBY | Motor enable — not wired, pulled HIGH by onboard resistor |

> ⚠️ VM carries battery voltage (12V+). If it bridges to any logic signal pin (AIN1/AIN2/BIN1/BIN2), the ESP32 GPIO is destroyed instantly. Keep VM wiring physically separate from signal wiring.

### Motor Direction Truth Table

| AIN1 / BIN1 | AIN2 / BIN2 | State |
|---|---|---|
| HIGH | LOW | Forward |
| LOW | HIGH | Reverse |
| LOW | LOW | Coast |
| HIGH | HIGH | Brake |

---

## PWM Configuration

- **Use 1 kHz PWM** — validated production setting for this chassis (JGA25-371 motors, TB6612FNG driver).
- **Do not use 20 kHz** — tested on this hardware and caused approximately 10× measured speed loss. Root cause not fully characterized, but the effect was consistent and repeatable. Do not change without a controlled re-validation on this chassis.
- 8-bit resolution (256 steps) at 1 kHz on ESP32 LEDC.
- PWM duty cycle maps linearly to motor speed.
- The PID controller outputs a target velocity in rad/s; the firmware maps this to a PWM duty cycle.

```cpp
// Correct LEDC setup — 1 kHz, 8-bit (validated on this chassis)
ledcSetup(0, 1000, 8);   // channel 0, 1 kHz, 8-bit — right motor
ledcSetup(1, 1000, 8);   // channel 1, 1 kHz, 8-bit — left motor
ledcAttachPin(PWMA, 0);
ledcAttachPin(PWMB, 1);
```

> ⚠️ **EMI note:** The left encoder (GPIO 40/41) can pick up noise from motor switching. Hardware 100 nF caps from GPIO 40/41 to GND are required in the signal path. Firmware uses PCNT `setFilter(400)` for additional hardware glitch rejection. Software EMA (`VEL_ALPHA`) is disabled — do not reintroduce it.

---

## Power Notes

- VM decoupling: **1000µF electrolytic + 100nF ceramic** in parallel, placed close to the VM and GND pins. Motor direction reversals and braking inject voltage spikes back onto the battery rail — the 1000µF cap absorbs them before they reach the Pi power circuit or INA219.
- Keep motor output wires (AO1/AO2, BO1/BO2) short and physically separated from signal wires.

---

## Common Issues

| Symptom | Likely Cause |
|---|---|
| Motor twitching / erratic PWM | Missing or floating common ground |
| No motor movement | STBY pin not pulled HIGH |
| Motor runs one direction only | AIN1/AIN2 swapped or stuck |
| Overheating | Continuous current exceeds 1.2A; check load / add heatsink |
