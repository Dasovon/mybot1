# Wheel Encoders — Odometry

## Role in This Project

Wheel encoders provide tick-level position and velocity feedback to the ESP32-S3. This data drives the PID velocity controller and generates raw odometry published to the ROS 2 graph as `/odom` (after fusion via `robot_localization`).

---

## Encoder Type

Quadrature encoders (2-channel, A + B per wheel) provide:
- Direction detection (forward / reverse)
- Higher effective resolution via 4× decoding

---

## Wiring — Confirmed Pin Assignments

**Motor/encoder model:** DC 12V JGA25-371, 45:1 gear ratio (Amazon listing says 34:1 — inaccurate).
Encoder: 11 PPR at motor shaft. Effective CPR with 2× quadrature decoding: **1010** (validated).

### Encoder Wire Colors (JGA25-371 6-wire harness)

| Wire Color | Function |
|---|---|
| Red | Motor power + |
| White | Motor power − |
| Blue | Encoder VCC (3.3V–5V) |
| Black | Encoder GND |
| Yellow | Encoder channel A |
| Green | Encoder channel B |

Blue (encoder VCC) is powered from ESP32 3V3. Encoder output signals are 3.3V compatible — no level shifter needed.

### Connection to ESP32-S3

Pins configured `INPUT_PULLUP`. Interrupt fires on CHANGE of the A channel only.

| ESP32 GPIO | Signal | Function |
|---|---|---|
| GPIO 40 | Left encoder A | `attachInterrupt` CHANGE |
| GPIO 41 | Left encoder B | Read in ISR for direction |
| GPIO 42 | Right encoder A | `attachInterrupt` CHANGE |
| GPIO 39 | Right encoder B | Read in ISR for direction |

ISR direction logic (validated):
- Left: `A == B on CHANGE` → forward (+)
- Right: `A != B on CHANGE` → forward (+)

### Key Constants

| Parameter | Value | Source |
|---|---|---|
| `ENC_CPR` | 1010 | Validated (3 runs: 1006/1016/1012 avg) |
| `wheel_radius` | 0.034 m | Measured (68 mm dia; datasheet 65 mm) |
| `wheel_separation` | 0.179 m | Measured center-to-center |

> ⚠️ **Known EMI issue:** GPIO 40/41 (left encoder A/B) pick up 1 kHz PWM noise from the TB6612.
> EMA filter (`VEL_ALPHA = 0.2`) mitigates it in firmware.
> Permanent hardware fix: solder 100 nF ceramic caps from GPIO 40 → GND and GPIO 41 → GND at the ESP32 headers.

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

The `esp32_serial_bridge` ROS node converts this to a `nav_msgs/Odometry` message on `/odom`.

---

## Notes

- Encoders connected directly to ESP32 GPIO — no separate encoder IC required.
- Ensure encoder signal voltage matches ESP32 GPIO input voltage (3.3V logic).
- If using 5V encoders, use a level shifter on signal lines.
- Shield encoder wires from motor power lines to prevent noise.
