# Wheel Encoders — Odometry

## Role in This Project

Wheel encoders provide tick-level position and velocity feedback to the ESP32-S3. This data drives the PID velocity controller and generates raw odometry published to the ROS 2 graph as `/odom` (after fusion via `robot_localization`).

---

## Encoder Type

Quadrature encoders (2-channel, A + B per wheel) provide:
- Direction detection (forward / reverse)
- Higher effective resolution via 4× decoding

---

## Connections to ESP32-S3

| Signal | Description | ESP32 Pin Requirement |
|---|---|---|
| Left A | Left encoder channel A | Hardware interrupt capable GPIO |
| Left B | Left encoder channel B | Hardware interrupt capable GPIO |
| Right A | Right encoder channel A | Hardware interrupt capable GPIO |
| Right B | Right encoder channel B | Hardware interrupt capable GPIO |
| VCC | Encoder power | 3.3V or 5V (check encoder spec) |
| GND | Common ground | Shared GND |

All ESP32-S3 GPIO pins support interrupts, but avoid strapping pins.

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
