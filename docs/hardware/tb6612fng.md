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

## Pin Connections to ESP32-S3

| TB6612FNG Pin | Connects To | Description |
|---|---|---|
| PWMA | ESP32 GPIO (PWM) | Left motor speed |
| AIN1 | ESP32 GPIO | Left motor direction bit 1 |
| AIN2 | ESP32 GPIO | Left motor direction bit 2 |
| PWMB | ESP32 GPIO (PWM) | Right motor speed |
| BIN1 | ESP32 GPIO | Right motor direction bit 1 |
| BIN2 | ESP32 GPIO | Right motor direction bit 2 |
| STBY | ESP32 GPIO (pull HIGH) | Standby — HIGH = enabled |
| GND | Common GND | Must share ground with ESP32 and battery |
| VM | Battery positive | Motor power supply |
| VCC | 3.3V or 5V | Logic supply from ESP32 board |
| AO1, AO2 | Left motor terminals | Motor A output |
| BO1, BO2 | Right motor terminals | Motor B output |

---

## Motor Direction Logic

| AIN1 | AIN2 | Motor State |
|---|---|---|
| HIGH | LOW | Forward |
| LOW | HIGH | Reverse |
| HIGH | HIGH | Brake |
| LOW | LOW | Coast |

Same logic applies to BIN1/BIN2 for the right motor.

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
