# Wheel Encoders — Odometry

## Role in This Project

Wheel encoders provide tick-level position and velocity feedback to the ESP32-S3. This data drives the PID velocity controller and generates raw odometry published to the ROS 2 graph as `/odom` (after fusion via `robot_localization`).

---

## Encoder Type

Quadrature encoders (2-channel, A + B per wheel) provide:
- Direction detection (forward / reverse)
- Higher effective resolution via 4× decoding

---

## Encoder Harness (JGA25-371 6-wire)

**Motor/encoder model:** DC 12V JGA25-371, 45:1 gear ratio (Amazon listing says 34:1 — inaccurate).
Encoder: 11 PPR at motor shaft. Effective CPR with 2× quadrature decoding: **1010** (validated).

| Wire Color | Function |
|---|---|
| Red | Motor power + |
| White | Motor power − |
| Blue | Encoder VCC (3.3V–5V) |
| Black | Encoder GND |
| Yellow | Encoder channel A |
| Green | Encoder channel B |

Encoder output is 3.3V compatible — no level shifter needed. Encoder VCC powered from ESP32 3V3.

ISR direction logic: Left `A == B on CHANGE` → forward (+) | Right `A != B on CHANGE` → forward (+)

GPIO assignments: GPIO 40/41 = Left A/B | GPIO 42/39 = Right A/B. See `docs/hardware/esp32_s3.md`.

> ⚠️ GPIO 40/41 pick up 20 kHz PWM switching noise from the TB6612. EMA filter `VEL_ALPHA = 0.2` in firmware. Hardware fix: 100 nF ceramic caps on GPIO 40 and 41 to GND, placed close to the ESP32 pins.

## Motor Performance (25SG-370CA-45-EN, 12V 4W)

From manufacturer datasheet:

| Condition | Speed | Current | Torque |
|---|---|---|---|
| No load | 190 rpm | < 150 mA | — |
| Rated | 100 rpm | < 750 mA | 5 kg·cm |
| Stall | 0 rpm | — | 9 kg·cm |

Max continuous current per motor: 750 mA. Two motors = 1.5A continuous from battery. Size battery and INA219 accordingly.

---

## Key Constants

| Parameter | Value | Source |
|---|---|---|
| `ENC_CPR` | 1010 | Validated (3 runs: 1006/1016/1012 avg) |
| `wheel_radius` | 0.034 m | Measured (68 mm dia; datasheet 65 mm) |
| `wheel_separation` | 0.179 m | Measured center-to-center |

> ⚠️ **Known EMI issue:** GPIO 40/41 (left encoder A/B) pick up 20 kHz PWM switching noise from the TB6612.
> EMA filter (`VEL_ALPHA = 0.2`) mitigates it in firmware.
> Hardware fix: route left encoder wires through a breadboard; place 100 nF ceramic caps from GPIO 40 → GND and GPIO 41 → GND on the breadboard before connecting to the ESP32.

---

## Firmware Implementation

- Attach interrupts to both A and B channels for each wheel.
- Use `CHANGE` interrupt mode to catch both edges for 4× resolution.
- Increment or decrement a tick counter based on A/B phase relationship.
- Compute velocity in the 100 Hz fast loop: `velocity = (delta_ticks / ticks_per_rev) * 2π / dt`

---

## Key Parameters (to be measured and set in firmware/config)

| Parameter | Description |
|---|---|
| `ticks_per_revolution` | Encoder counts per full wheel rotation |
| `wheel_radius_m` | Wheel radius in meters |
| `wheel_base_m` | Distance between left and right wheel centers |

These values must be measured precisely on the physical robot and set in the firmware and in the URDF.

---

## Odometry Telemetry

The ESP32 publishes raw tick counts at 100 Hz:

```
ENC <left_ticks> <right_ticks>
```

The `esp32_serial_bridge` ROS node converts this to a `nav_msgs/Odometry` message on `/diff_cont/odom`.

---

## Notes

- Encoders connected directly to ESP32 GPIO — no separate encoder IC required.
- Ensure encoder signal voltage matches ESP32 GPIO input voltage (3.3V logic).
- If using 5V encoders, use a level shifter on signal lines.
- Shield encoder wires from motor power lines to prevent noise.
