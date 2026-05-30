# PID & Encoder Debug Session — 2026-05-30

## Session Goal
Pass the Phase 1 PID velocity gate: wheel velocity tracks 0.10 m/s command at 30 Hz, straight line, within ±10%.

## Hardware State at Session Start
- Test chassis: 2-wheel diff-drive + rear caster, 1 motor per side
- Motor driver: TB6612FNG, PWM at 1 kHz, GPIO 10–15
- Encoders: JGA25-371, GPIO 39–42, ESP32Encoder (PCNT half-quad)
- Firmware: micro-ROS on native USB CDC → Pi `/dev/ttyACM0`
- CH340 (`/dev/ttyUSB0`) NOT connected — Serial0 debug inaccessible

---

## Issues Found and Fixed This Session

### 1. Multiple micro-ROS Agent Instances (ROOT CAUSE of prior "PCNT returns 0")
**Problem:** Three instances of `micro_ros_agent` were fighting over `/dev/ttyACM0`, corrupting the XRCE session. Odom and encoder data appeared as all-zeros because the serial stream was garbled.

**Fix:** Always stop `microros-agent.service` before killing individual processes; never launch additional agents while the service is active. One instance = one session.

**Verification:** Push test after single-agent cleanup → position accumulated to 0.9588 m. Encoders confirmed working.

---

### 2. Encoder A/B Wires Swapped at Breadboard (Both Sides)
**Problem:** When routing encoder signal lines through the breadboard to install EMI caps, both the left encoder (GPIO 40/41) and right encoder (GPIO 42/39) had Yellow (Ch A) and Green (Ch B) wires swapped. Effect: one encoder counted negative while the other counted positive → odom velocity = (positive + negative)/2 ≈ 0, robot turned instead of going straight.

**Fix:** Swapped both pairs back: Yellow → A pin, Green → B pin for each encoder.

**Verification:** Push test forward-only → position peaked at +0.0602 m, result PASS.

---

### 3. Motor Driver Power Not Connected
**Problem:** TB6612FNG VM (motor power rail) was not plugged in during several test runs. Motors received no power; robot did not move.

**Fix:** Reconnected motor power rail.

---

### 4. Pi Filesystem Read-Only After Power Cycle
**Problem:** Ubuntu's `systemd-remount-fs.service` takes ~3.5 minutes to remount the NVMe root filesystem read-write after a cold boot. If SSH sessions start before that, any write (ros2 log files, test captures) fails with "Read-only file system", causing ros2 tools to crash.

**Fix (temporary):** `sudo mount -o remount,rw /dev/nvme0n1p2 /` immediately after booting.

**Note:** fstab uses `LABEL=writable` but kernel cmdline uses `LABEL=nvme-writable` — these labels mismatch. systemd eventually resolves it at ~217s but the mismatch causes the `mount -o remount,rw /` CLI command to fail (must specify device path, not label).

---

## CLAUDE.md Rules Added This Session
Four new sections added under "Rules for Claude Code":
- **Motor Test Safety Rules** — use `--times N`, not `timeout`; always send explicit zero; 5s max for velocity test; confirm stop before analysis
- **Physical Action Rules** — state action, stop, wait for explicit user acknowledgement
- **Test Reporting Rules** — all four elements every time: test ran / results / expected / measured
- **Before Changing Things** — pause, explain options, wait for user direction

---

## Firmware Changes Since Last Commit

| File | Change | Reason |
|---|---|---|
| `main.cpp` | `ENC_CPR_F` 1010 → 990 | Matches floor-validated CPR and old bot value |
| `main.cpp` | Added `vel_r_filt`, `vel_l_filt` EMA (VEL_ALPHA=0.2) | Attempt to smooth EMI noise on GPIO 40/41 |
| `main.cpp` | `Serial0.begin(115200)` moved before `encoders_init()` | Allows PCNT init logs to print (Serial0 must init first) |
| `main.cpp` | Added 2 Hz raw PCNT count debug log | Diagnose encoder counting via CH340 when connected |
| `main.cpp` | Switched from deadband floor to feedforward + PID | ff=0.25 ensures stiction overcome; PID trims residual |
| `main.cpp` | `MOTOR_MAX_RAD_S` 6.5 → 3.0 | Measured max on test chassis is ~2.93 rad/s |
| `main.cpp` | Velocity computed from EMA not raw delta | Consistent with filtered PID feedback |
| `main.cpp` | Watchdog branch resets `vel_r_filt`/`vel_l_filt` | Prevents stale filter state after stop |
| `encoders.cpp` | Added Serial0.printf after each attachHalfQuad | Confirm PCNT init ran and shows initial counts |
| `pid.h` | `KI` 1.5 → 3.0, `INTEGRAL_LIMIT` 40 → 2.0 | Tighter integrator for feedforward architecture |
| `motors.h` | `MOTOR_MIN_DUTY` → `MOTOR_FEEDFORWARD`, comment updated | Renamed to reflect additive feedforward role |

**All changes are uncommitted.** Current firmware IS flashed on the ESP32.

---

## 100nF EMI Caps — Status

Installed on all four encoder signal lines:
- GPIO 40 (Left A, Yellow) → GND
- GPIO 41 (Left B, Green) → GND
- GPIO 42 (Right A, Yellow) → GND
- GPIO 39 (Right B, Green) → GND

**Push test result with caps:** PASS — forward push = positive position, 400 samples nonzero.

---

## Remaining Blocker: Encoders Return Zero During Motor Operation

### Confirmed Behavior
| Condition | Encoder counts |
|---|---|
| Manual push (no motors) | ✓ Working — position tracks correctly |
| Motor-driven (0.10 m/s cmd_vel) | ✗ All zeros — 630 samples, zero deviation |

### Diagnostic Data (Last Motor Test)
| Measurement | Value | Interpretation |
|---|---|---|
| odom pos.x delta | 0.00000 m | PCNT net = 0 during motor run |
| odom vel.x | 0.0000 m/s all samples | No velocity feedback reaching PID |
| IMU yaw rate (ang_vel.z) | −0.003 to +0.003 rad/s | Robot going straight ✓ (encoder fix worked) |
| IMU lin_accel.x | −0.090 to −0.030 m/s² | Slight gravity offset; no large acceleration |
| Battery voltage | Stable | No power anomalies |

### Root Cause Hypothesis
The ESP32Encoder PCNT half-quad mode creates equal forward and backward spurious counts under TB6612FNG 1 kHz PWM switching noise. Net PCNT delta per 10ms window ≈ 0. The 100nF caps provide some filtering but the noise path may also run through the motor power rails or ground plane, bypassing the GPIO-to-GND caps.

### Why Push Test Works But Motor Test Doesn't
During a manual push there is no PWM switching on the motor driver. Encoder signal integrity is high. PCNT counts cleanly in one direction.

### Options for Next Session

**Option A — Switch to ISR-based counting (like old bot)**
The `dev_ws/articubot_one` firmware used `attachInterrupt` + CHANGE on pin A + directional read of B. It worked with VEL_ALPHA=0.3. ISR approach may handle the 1kHz noise differently than PCNT quadrature mode. This is a firmware-only change and can be tested immediately.

**Option B — Enable PCNT hardware glitch filter**
ESP32Encoder exposes `setFilter(value)` — sets minimum pulse width (N × 12.5 ns) the PCNT will accept. A value of ~400 (5 µs) would reject 1kHz noise pulses (~500 µs period) while passing legitimate encoder pulses (462 Hz → 2.16 ms period). This is a 2-line firmware change.

**Option C — Hardware: add series resistors + lower-value caps**
100nF with weak pull-ups (45kΩ) gives RC cutoff ≈ 35 Hz, potentially too aggressive. Replacing with 4.7nF caps raises cutoff to ~750 Hz while still filtering 1 kHz noise. Requires hardware change.

**Recommended first step: Option B (PCNT glitch filter)** — 2-line change, no hardware, low risk. If that fails, try Option A (ISR switch).

---

## System State for Next Session

### Pi Setup Required
```bash
# After boot, wait ~30s then remount filesystem:
sudo mount -o remount,rw /dev/nvme0n1p2 /

# Verify single agent (2 PIDs = 1 instance, normal for ros2 run):
pgrep -af micro_ros_agent   # should show python launcher + C++ binary only

# If multiple real instances:
sudo systemctl stop microros-agent.service
sudo systemctl start microros-agent.service

# Verify odom live before any test:
source /opt/ros/jazzy/setup.bash && source ~/microros_ws/install/setup.bash
ros2 topic echo /diff_cont/odom --once
```

### Pre-Test Checklist
- [ ] Motor driver VM power rail plugged in
- [ ] All four encoder signal wires seated: Yellow=A, Green=B on each encoder
- [ ] Single agent instance running
- [ ] Odom publishing (`frame_id: odom` visible)
- [ ] Battery voltage > 10.2V

### Test Procedure (5-second velocity test)
```bash
# 1. Start odom capture (separate SSH):
ssh ubuntu@pi5bot 'source /opt/ros/jazzy/setup.bash && source ~/microros_ws/install/setup.bash && \
  timeout 10 ros2 topic echo /diff_cont/odom > ~/test_logs/odom_$(date +%H%M%S).txt 2>/dev/null'

# 2. Run motors (100 msgs × 20 Hz = 5s):
ssh ubuntu@pi5bot 'source /opt/ros/jazzy/setup.bash && source ~/microros_ws/install/setup.bash && \
  ros2 topic pub --times 100 --rate 20 /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist \
  "{linear: {x: 0.10}}" > /dev/null 2>&1'

# 3. Explicit stop:
ssh ubuntu@pi5bot 'source /opt/ros/jazzy/setup.bash && source ~/microros_ws/install/setup.bash && \
  ros2 topic pub --times 10 --rate 20 /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist \
  "{linear: {x: 0.0}}" > /dev/null 2>&1'
```
