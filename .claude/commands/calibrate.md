# /calibrate

Calibration workflows for encoders, PID velocity control, and wheel geometry.
Run these in order when setting up Phase 1 or after any hardware change.

## Usage
`/calibrate [encoders|pid|geometry]`

---

## 1 — Encoder CPR validation

Confirms `ENC_CPR = 1010` is correct for your specific motor/encoder pair.

```bash
# On Pi: monitor the raw encoder topic while you manually rotate one wheel
ros2 topic echo /diff_cont/odom

# OR: use serial monitor during firmware test
# Manually rotate wheel exactly 1 full revolution
# Count encoder pulses reported
# Expected: 1010 counts (2× quadrature × 45:1 gear ratio × 11.2 CPR base)
# If count differs by >2%, update ENC_CPR in firmware and hardware_constants.md
```

Hardware constants (do not change without re-validating on floor):
- `ENC_CPR = 1010`
- `wheel_radius = 0.034 m` (measured; datasheet is 0.0325 m)
- `wheel_separation = 0.179 m` (measured center-to-center)

---

## 2 — Wheel geometry validation

Confirms wheel_radius and wheel_separation are correct by measuring real motion.

```bash
# Test 1: straight-line distance
# Command robot forward at 0.2 m/s for 5 seconds → expect 1.0 m
# Measure actual distance with tape measure
# If off: adjust wheel_radius = wheel_radius × (measured / expected)

# Test 2: 360° rotation
# Command robot to rotate 2π rad in place
# Robot should return to exact start heading
# If off: adjust wheel_separation = wheel_separation × (commanded_angle / actual_angle)
```

---

## 3 — PID velocity tuning (closed-loop)

Tune in this order: P only → add I → add D if needed.

```bash
# Monitor wheel velocity vs command
ros2 topic echo /diff_cont/odom   # watch twist.twist.linear.x

# Recommended starting point (from build_plan.md):
# Kp = 1.5, Ki = 0.5, Kd = 0.0
# - If oscillating: reduce Kp
# - If steady-state error: increase Ki
# - If overshoot: add small Kd (0.01–0.05)
```

PID parameters live in: `firmware/esp32/src/` — change in firmware, reflash, observe.

Never tune PID with open-loop PWM. Always use encoder feedback.

---

## 4 — EMI check (GPIO 40/41 left encoder)

Left encoder picks up TB6612 1 kHz PWM noise. Verify hardware mitigation is working.

```bash
# Command robot to hold still (zero velocity)
# Monitor left encoder velocity output — should read 0.0 ± noise
ros2 topic echo /diff_cont/odom   # watch twist.twist.linear.y (or left wheel velocity)

# If noisy at standstill:
# 1. Add 100 nF ceramic caps: GPIO 40 → GND, GPIO 41 → GND
# 2. Verify VEL_ALPHA = 0.2 EMA filter is active in firmware
```

---

## Pass criteria
- [ ] ENC_CPR confirmed within 2% of 1010
- [ ] 1 m drive error < 2 cm
- [ ] 360° rotation error < 5°
- [ ] PID velocity tracks step command within 100 ms, no sustained oscillation
- [ ] Left encoder reads < 0.01 rad/s noise at standstill
