# PID Tuning Guide

**Date:** 2026-05-18
**Related automation tool:** `scripts/pid_tuner.py` (to be built — see note at bottom of each stage)
**Applies to:** ESP32-S3 velocity PID controller, both wheels independently

---

## Overview

The robot uses closed-loop PID velocity control on each wheel independently. The PID runs at 100 Hz on the ESP32. Encoder feedback (1010 CPR) provides measured wheel velocity in rad/s. The controller output drives the TB6612 PWM signal.

Each wheel has its own PID instance with its own gains (Kp, Ki, Kd). Tune them separately, then verify they track together.

**Tuning order:**
1. Closed-loop system characterization
2. P-only (Kp)
3. PD (add Kd)
4. Full PID (add Ki)
5. Symmetric tuning (right then left)
6. Velocity range sweep
7. Direction change test
8. Straight-line and rotation validation
9. ROS integration test

Do not skip stages. Each stage's pass criteria are prerequisites for the next.

---

## Robot Constants (do not change during tuning)

| Parameter | Value |
|---|---|
| `ENC_CPR` | 1010 counts/rev |
| `wheel_radius` | 0.034 m |
| `wheel_separation` | 0.179 m |
| PID loop rate | 100 Hz |
| PWM frequency | 20 kHz (LEDC) |
| PWM resolution | 8-bit (0–255) |
| Motor deadband | TBD — measured in Stage 1 |
| Max wheel velocity | TBD — measured in Stage 1 |

---

## Firmware Requirements for Tuning

Before tuning can begin, the ESP32 firmware must:

1. **Expose PID gains at runtime** — gains must be adjustable without reflashing. Implement a serial command interface (or micro-ROS parameter service) that accepts new Kp, Ki, Kd values and applies them immediately:
   ```
   Serial command format:  PID:R:KP:0.50   (right wheel Kp = 0.50)
                           PID:L:KI:0.01   (left wheel Ki = 0.01)
                           PID:R:KD:0.05   (right wheel Kd = 0.05)
   ```

2. **Stream telemetry at 100 Hz** — publish the following over serial (or micro-ROS) for the Python tool to record:
   ```
   TELEM:<timestamp_ms>:<r_setpoint_rads>:<r_measured_rads>:<r_pwm>:<l_setpoint_rads>:<l_measured_rads>:<l_pwm>
   ```

3. **Accept velocity setpoints independently per wheel** — for per-wheel tests, allow setting right and left wheel velocity targets independently (not just as a combined twist command).

---

## Software Prerequisites

### On development PC

| Tool | Purpose | Install |
|---|---|---|
| Python 3.10+ | Tuning automation | https://www.python.org/downloads/ |
| pip | Package manager | Included with Python |
| matplotlib | Response curve plotting | `pip install matplotlib` |
| numpy | Signal analysis | `pip install numpy` |
| pandas | Data logging | `pip install pandas` |
| pyserial | Serial communication | `pip install pyserial` |
| rclpy | ROS 2 Python client | Included with ROS 2 Jazzy |

### On Raspberry Pi

- ROS 2 Jazzy (installed in Gate 2 of electronics protocol)
- micro-ROS agent running (see Gate 13)

### Serial monitor (for manual tuning without automation)
```bash
# On Pi, monitor ESP32 telemetry:
screen /dev/ttyACM0 115200
# or
python3 -m serial.tools.miniterm /dev/ttyACM0 115200
```

---

## Stage 1 — Closed-Loop System Characterization

**Goal:** With the PID active and encoders providing feedback, characterize the system's velocity range and step response. This data sets a rational starting point for Kp. The PID is running the entire time — no raw PWM mode, no disabling the controller.

Start with a very low Kp (Ki = 0, Kd = 0). Low enough that the system responds slowly but does not oscillate. This gives clean, readable step responses to measure from.

**Starting gains for this stage:**
```
Kp = 0.3
Ki = 0.0
Kd = 0.0
```

**What the Python tool will do:** Command a sweep of velocity setpoints, record encoder velocity via telemetry stream, plot setpoint vs. measured velocity curves, calculate minimum controllable velocity, max velocity, and time constant (τ).

### Manual procedure

**Step 1.1 — Minimum controllable velocity**

Command increasing velocity setpoints from 0.5 rad/s upward in 0.5 rad/s steps. At each setpoint, wait 1 second and read the steady-state encoder velocity from the telemetry stream.

```
VEL:R:0.5   → read measured vel after 1s
VEL:R:1.0   → read measured vel after 1s
VEL:R:1.5   → read measured vel after 1s
...
```

The minimum controllable velocity is the lowest setpoint at which the encoder reliably tracks (measured velocity within 20% of setpoint). Below this, static friction prevents consistent control. Record it.

Expected minimum: ~1–3 rad/s for these gear motors at low Kp.

**Step 1.2 — Maximum velocity**

Command increasing setpoints from minimum up to where the measured velocity stops increasing. This is the **maximum closed-loop velocity** — encoder-confirmed, not assumed.

```
VEL:R:5.0   → record measured vel
VEL:R:8.0   → record measured vel
VEL:R:12.0  → record measured vel
VEL:R:16.0  → record measured vel
...          → find where velocity saturates
```

Record max velocity for each wheel separately. They may differ slightly.

**Step 1.3 — Step response (time constant)**

Command a step from 0 → 5 rad/s. Record encoder velocity from the telemetry stream until steady state. The time from command to 63% of final measured velocity is the system **time constant (τ)**.

```
VEL:R:0.0   → confirm stationary
VEL:R:5.0   → start recording
             → note time at 63% of steady-state velocity
```

Expected τ: 100–300ms under closed-loop control with low Kp.

**Pass criteria:**
- [ ] Minimum controllable velocity measured — right wheel: _____ rad/s
- [ ] Minimum controllable velocity measured — left wheel: _____ rad/s
- [ ] Max velocity measured — right wheel: _____ rad/s
- [ ] Max velocity measured — left wheel: _____ rad/s
- [ ] Time constant measured: _____ ms
- [ ] Velocity response curves plotted and saved for both wheels

**Note for Python tool:** Command the velocity sweep automatically, capture the telemetry stream, fit the step response to a first-order model, report min velocity, max velocity, and τ. These values feed directly into Stage 2's Kp starting point calculation.

---

## Stage 2 — P-Only Tuning (Kp)

**Goal:** Find a Kp that gives a fast rise to setpoint without excessive oscillation. Ki = 0, Kd = 0.

**Starting point** (derived from Stage 1 results):
```
Kp = 1.0 / (τ_seconds * max_vel_rads)
   e.g. τ=0.2s, max_vel=18 rad/s → Kp ≈ 0.28 — round up to 0.3 and increase from there
Ki = 0
Kd = 0
```

### Manual procedure

**Step 2.1 — Set initial gains via serial:**
```
PID:R:KP:0.5
PID:R:KI:0.0
PID:R:KD:0.0
```

**Step 2.2 — Step response test**

Command right wheel to a moderate target velocity (e.g. 5 rad/s). Watch the telemetry stream for:
- **Rise time** — how fast it reaches the target
- **Overshoot** — how far above target it goes
- **Oscillation** — does it settle, or keep bouncing?

Repeat with increasing Kp. Stop when oscillation becomes continuous (this is Ku — the ultimate gain).

Target Kp = 0.5 × Ku (back off from oscillation point).

| Kp | Rise time | Overshoot | Settled? |
|---|---|---|---|
| (fill in during test) | | | |

**Step 2.3 — Ziegler-Nichols starting point (alternative)**

If step 2.2 is unclear, use Ziegler-Nichols:
1. Find Ku (Kp at which sustained oscillation occurs)
2. Measure Tu (oscillation period in seconds)
3. Starting gains:
   - P-only: `Kp = 0.5 × Ku`
   - PID: `Kp = 0.6 × Ku`, `Ki = 1.2 × Kp / Tu`, `Kd = 0.075 × Kp × Tu`

Reference: https://en.wikipedia.org/wiki/Ziegler%E2%80%93Nichols_method

**Pass criteria:**
- [ ] Step response rises to target in < 200ms
- [ ] Overshoot < 30% with P-only (some overshoot is expected without Kd)
- [ ] No continuous oscillation
- [ ] Kp value recorded: _____

**Note for Python tool:** Send step command, record telemetry, calculate rise time, overshoot, and settling time automatically. Plot response curve. Iterate Kp values and overlay responses on one graph.

---

## Stage 3 — PD Tuning (Add Kd)

**Goal:** Add derivative term to reduce overshoot and dampen oscillation without slowing the rise too much.

**Starting point:** `Kd = 0.1 × Kp` (from Stage 2 result)

### Manual procedure

**Step 3.1 — Set gains:**
```
PID:R:KP:<kp_from_stage2>
PID:R:KI:0.0
PID:R:KD:<starting_kd>
```

**Step 3.2 — Step response test**

Command step to 5 rad/s. Compare to Stage 2 result:
- Overshoot should decrease
- Rise time may increase slightly (acceptable)
- No oscillation at steady state

Increase Kd gradually. Stop when:
- Response becomes sluggish or rise time > 300ms, or
- High-frequency oscillation appears (Kd too high — amplifying encoder noise)

**Step 3.3 — Noise sensitivity check**

With high Kd, high-frequency PWM noise on GPIO 40/41 (left encoder) can cause chattering. Watch PWM output in telemetry — it should not oscillate rapidly at steady state. If it does: reduce Kd, or verify EMI caps from Gate 8 are in place.

| Kd | Overshoot | Rise time | Chattering? |
|---|---|---|---|
| (fill in during test) | | | |

**Pass criteria:**
- [ ] Overshoot < 10% with PD control
- [ ] Rise time < 300ms
- [ ] No PWM chattering at steady state
- [ ] Kd value recorded: _____

**Note for Python tool:** Same step test infrastructure as Stage 2. Add overshoot measurement and chattering detection (high-frequency PWM variance). Overlay PD vs P-only responses on one plot.

---

## Stage 4 — Full PID Tuning (Add Ki)

**Goal:** Add integral term to eliminate steady-state error. Without Ki, the wheel may settle 5–15% below target due to friction.

**Starting point:** `Ki = 0.1 × Kp / τ` where τ is the time constant from Stage 1.

⚠️ **Integral windup warning:** If the motor is stalled (robot physically blocked) and Ki is high, the integrator will wind up to maximum. The firmware must implement integrator clamping:
```cpp
// Clamp integral term to ±MAX_PWM
integral = constrain(integral, -255, 255);
```
Confirm this is in the firmware before increasing Ki.

### Manual procedure

**Step 4.1 — Set gains:**
```
PID:R:KP:<kp>
PID:R:KI:<starting_ki>
PID:R:KD:<kd_from_stage3>
```

**Step 4.2 — Steady-state error test**

Command 5 rad/s. Wait 2 seconds. Read steady-state velocity. With Ki = 0 there will be some error. As Ki increases, error approaches 0.

Also test at low velocity (2 rad/s) — steady-state error is more pronounced near the motor deadband.

**Step 4.3 — Windup recovery test**

1. Command 5 rad/s.
2. Hold the wheel stationary with your hand for 3 seconds (integrator winds up).
3. Release.
4. Wheel should return to target quickly without violent overshoot.

If wheel surges wildly after release: reduce Ki or tighten integral clamp limits.

**Step 4.4 — Ramp test**

Command a series of increasing velocities (2 → 4 → 6 → 8 rad/s in 1-second steps). Verify the PID tracks smoothly without oscillation at any step.

| Ki | Steady-state error | Windup recovery | Ramp tracking |
|---|---|---|---|
| (fill in during test) | | | |

**Pass criteria:**
- [ ] Steady-state error < 2% of setpoint at 5 rad/s
- [ ] Steady-state error < 5% of setpoint at 2 rad/s
- [ ] Windup recovery: no violent surge after stall release
- [ ] Ramp test: smooth tracking, no oscillation
- [ ] Ki value recorded: _____
- [ ] Final PID gains recorded: Kp=_____ Ki=_____ Kd=_____

**Note for Python tool:** Test steady-state error automatically by averaging velocity over the final 500ms of a 2-second step. Run the stall/release test with a timer. Flag any post-stall overshoot > 15%.

---

## Stage 5 — Symmetric Tuning (Right Then Left)

**Goal:** Both wheels use separately tuned gains. They should track the same setpoint identically.

### Manual procedure

**Step 5.1 — Tune right wheel (Stages 1–4 results)**

Right wheel gains are from Stages 1–4. Apply them:
```
PID:R:KP:<kp_R>
PID:R:KI:<ki_R>
PID:R:KD:<kd_R>
```

**Step 5.2 — Repeat Stages 1–4 for left wheel**

Left wheel may need different gains due to:
- Different friction (bearing wear, load)
- EMI on GPIO 40/41 increasing effective noise floor
- Slight motor manufacturing variance

Repeat the full tuning sequence for the left wheel:
```
PID:L:KP:<starting_kp>
PID:L:KI:0.0
PID:L:KD:0.0
```

Work through Stages 2, 3, 4 for left wheel independently.

**Step 5.3 — Tracking comparison**

Command both wheels to the same setpoint simultaneously. Plot right and left velocity on the same graph. They should converge to the same steady-state value within 200ms of each other.

Command: both wheels at 5 rad/s. Measure:
- Time for each to reach 90% of target
- Steady-state difference between right and left

**Pass criteria:**
- [ ] Right wheel: Kp=_____ Ki=_____ Kd=_____
- [ ] Left wheel: Kp=_____ Ki=_____ Kd=_____
- [ ] Both reach steady state within 200ms of each other
- [ ] Steady-state velocity difference < 3% at 5 rad/s
- [ ] Left encoder EMI does not cause visible chattering in left wheel response

**Note for Python tool:** Plot both wheels overlaid on the same time axis. Report the tracking difference automatically.

---

## Stage 6 — Velocity Range Sweep

**Goal:** Verify PID performance across the full velocity range the robot will use during operation. PID gains that work at one speed may oscillate or underperform at another.

### Test velocities

| Name | Target (rad/s) | Equivalent linear (m/s) |
|---|---|---|
| Creep | 1.0 | 0.034 |
| Slow | 3.0 | 0.10 |
| Cruise | 6.0 | 0.20 |
| Fast | 12.0 | 0.41 |
| Max | ~18.0 (measure in Stage 1) | ~0.61 |

### Manual procedure

For each velocity, run a 3-second step response. Record:
- Rise time (0 → 90% of target)
- Overshoot
- Steady-state error
- Settling time

Use the full PID gains from Stage 5.

| Velocity (rad/s) | Rise time (ms) | Overshoot (%) | SS error (%) | Settled? |
|---|---|---|---|---|
| 1.0 | | | | |
| 3.0 | | | | |
| 6.0 | | | | |
| 12.0 | | | | |
| Max | | | | |

**Pass criteria:**
- [ ] No oscillation at any test velocity
- [ ] Overshoot < 15% at all velocities
- [ ] Steady-state error < 5% at all velocities
- [ ] Both wheels pass at all velocities

**If gains work at cruise but fail at creep:** The motor deadband is dominating at low speed. Add a feedforward deadband offset in the firmware to compensate before the PID output is applied.

**Note for Python tool:** Automate the full sweep. Run each velocity for 3 seconds, compute metrics, produce a table and overlay plot of all velocity responses.

---

## Stage 7 — Direction Change Test

**Goal:** Verify the PID handles setpoint sign reversal without windup surge or dangerous wheel behavior.

### Manual procedure

**Step 7.1 — Forward → stop:**
```
Command: 5 rad/s → 0 rad/s (step)
Expected: wheel decelerates smoothly, stops, does not back-creep
```

**Step 7.2 — Forward → reverse:**
```
Command: 5 rad/s → -5 rad/s (step)
Expected: wheel decelerates, passes through zero, accelerates in reverse
No violent current spike (watch for TB6612 heat)
```

**Step 7.3 — Rapid reversals (stress test):**
```
Alternate: +5 rad/s for 500ms → -5 rad/s for 500ms, 10 cycles
Expected: consistent behavior each cycle, no growing oscillation
```

**Pass criteria:**
- [ ] Stop from cruise: clean deceleration, no back-creep
- [ ] Full reversal: smooth, no violent surge
- [ ] Rapid reversals: stable for all 10 cycles
- [ ] TB6612 not hot after stress test

**Note for Python tool:** Automate the reversal pattern. Detect back-creep (velocity past zero at stop command). Detect surge (velocity spike > 20% above setpoint during transition).

---

## Stage 8 — Straight-Line and Rotation Test

**Goal:** Verify both wheels together produce accurate straight-line motion and in-place rotation under closed-loop wheel velocity control.

Robot must be on the floor for this stage. Measure with a tape measure or marked floor grid.

### Manual procedure

**Step 8.1 — Straight-line test**

Command both wheels to the same velocity (5 rad/s) for 3 seconds. Robot should drive approximately straight.

Measure lateral drift at 1 meter of travel. Accept up to 5 cm drift — gross drift beyond that indicates a wheel gain mismatch that needs correcting in the PID gains before moving on.

**Step 8.2 — Rotation test**

Command right wheel +5 rad/s, left wheel -5 rad/s (in-place spin). Run for 1 full rotation (360°). Measure actual rotation using a reference mark on the robot.

Expected rotation (one revolution):
```
arc_length = π × wheel_separation = π × 0.179 = 0.562 m
revolutions_per_wheel = arc_length / (2π × wheel_radius)
                       = 0.562 / (2π × 0.034) = 2.63 wheel revolutions
encoder_counts = 2.63 × 1010 = 2,657 counts per wheel
```

Count encoder pulses to verify actual rotation matches expected.

**Step 8.3 — Square path test**

Drive: forward 1m → rotate 90° right → forward 1m → rotate 90° right → repeat 4 times. Robot should return close to start position.

Acceptable return error at this stage: < 15 cm and < 15°.

**Pass criteria:**
- [ ] Straight-line drift < 5 cm per meter
- [ ] 360° rotation within ±15° of expected
- [ ] Square path return error < 15 cm, < 15°

**Note for Python tool:** Use `/diff_cont/odom` to track position during the square test. Plot the path and compute return error automatically.

---

## Stage 9 — ROS Integration Test

**Goal:** Verify PID performance is maintained when velocity commands arrive from ROS (via micro-ROS) rather than direct serial. Confirm `/diff_cont/odom` odometry matches expected motion.

### Software used

- micro-ROS agent (running on Pi — see Gate 13 of electronics protocol)
- ROS 2 topic tools

### Manual procedure

**Step 9.1 — Command via ROS topic**
```bash
source /opt/ros/jazzy/setup.bash

# Single velocity step command:
ros2 topic pub /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist \
    "{linear: {x: 0.2, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.0}}" \
    --rate 20 --times 60   # 3 seconds at 20 Hz
```

**Step 9.2 — Verify odom accuracy**

While driving at 0.2 m/s linear for 3 seconds:
```bash
ros2 topic echo /diff_cont/odom | grep -A3 "pose"
```

Expected displacement: ~0.6 m. Check `pose.pose.position.x` value after 3 seconds.

**Step 9.3 — Watchdog test via ROS**

Start publishing velocity commands at 20 Hz, then `Ctrl+C`. Robot must stop within 500ms. Confirm via odom that velocity returns to 0.

**Step 9.4 — cmd_vel rate test**

Verify `/diff_cont/cmd_vel_unstamped` is being received at the expected rate:
```bash
ros2 topic hz /diff_cont/cmd_vel_unstamped
```
Expected: ~20 Hz from Nav2 (confirmed later). For now, verify your test publisher sends at stable 20 Hz.

**Pass criteria:**
- [ ] Robot responds to ROS velocity commands correctly
- [ ] Odom displacement within 10% of expected after straight-line run
- [ ] Watchdog triggers within 500ms of ROS command stream stopping
- [ ] No PID behavior degradation vs. serial-commanded tests

**Note for Python tool:** Subscribe to `/diff_cont/odom`, publish to `/diff_cont/cmd_vel_unstamped`. Automate the accuracy test. Compare odom displacement vs. expected. This is the foundation for the automation tool's ROS integration.

---

## Final Gain Record

Fill in after Stage 9 passes. Commit this document with the completed values.

| Parameter | Right wheel | Left wheel |
|---|---|---|
| `Kp` | | |
| `Ki` | | |
| `Kd` | | |
| Deadband offset | | |
| Max velocity (rad/s) | | |
| Time constant τ (ms) | | |

These values go into the firmware as defaults in `firmware/esp32/include/pid.h`.

---

## Python Automation Tool Plan

**Location:** `scripts/pid_tuner.py`

**What it will do:**

| Feature | Description |
|---|---|
| Serial + ROS interface | Connects to ESP32 via serial for raw telemetry; uses ROS 2 to publish cmd_vel |
| Step test runner | Sends step commands, records response for a configurable duration |
| Metric calculator | Computes rise time, overshoot, settling time, steady-state error automatically |
| Gain sweeper | Iterates a range of Kp/Ki/Kd values, runs step test at each, ranks by score |
| Response plotter | Plots velocity vs. time with matplotlib; overlays multiple runs |
| Ziegler-Nichols helper | Detects oscillation period, suggests PID starting gains |
| Data logger | Saves all runs to CSV in `scripts/pid_logs/` for review |
| Report generator | Outputs a markdown summary of the best gains found |

**Dependencies** (install on dev PC):
```bash
pip install matplotlib numpy pandas pyserial
pip install rclpy  # or install via ROS 2 colcon
```

**Planned usage:**
```bash
# Run full auto-tune sequence for right wheel:
python3 scripts/pid_tuner.py --wheel right --stage all

# Tune Kp only:
python3 scripts/pid_tuner.py --wheel right --stage kp --kp-range 0.1 2.0 --steps 20

# Run step test with specific gains and plot:
python3 scripts/pid_tuner.py --wheel both --kp 0.8 --ki 0.05 --kd 0.1 --plot

# Run Stage 8 square path test via ROS:
python3 scripts/pid_tuner.py --stage square --plot-path
```

**Note:** The automation tool is a helper, not a replacement for the manual stages. Always verify results physically — the robot's behavior on the floor is the ground truth.

---

## References

- Ziegler-Nichols method: https://en.wikipedia.org/wiki/Ziegler%E2%80%93Nichols_method
- PID controller theory: https://en.wikipedia.org/wiki/Proportional%E2%80%93integral%E2%80%93derivative_controller
- LEDC API (ESP32 PWM): https://docs.espressif.com/projects/arduino-esp32/en/latest/api/ledc.html
- micro-ROS parameters: https://micro.ros.org/docs/tutorials/core/parameters/
