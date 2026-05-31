# mybot1 — Project Context

> **For AI assistants:** Standalone project. Do not reference other robot projects. All wiring data in `docs/hardware/` is authoritative. Read [`docs/architecture/build_plan.md`](docs/architecture/build_plan.md) before starting any implementation work.

---

## 1. Purpose

A distributed ROS 2 Jazzy autonomous mobile robot (AMR) — clean standalone build from scratch. Capable of autonomous mapping, SLAM, navigation, obstacle avoidance, multi-sensor fusion, and semantic perception. Architecture mirrors a commercial AMR, not a hobby robot.

---

## 2. Current Phase

**Phase 3 in progress — Sensor Bridge & EKF**

| Phase | Status |
|---|---|
| 0 — Hardware & Environment | Complete |
| 1 — ESP32 Firmware | **Complete** — P-only baseline validated; encoders, IMU, watchdog passing gate |
| 2 — ROS 2 Foundation (URDF + TF) | **Complete** — 9 frames, all named correctly, `base_footprint` root added |
| 3 — Sensor Bridge & EKF | **In progress** — EKF live; LiDAR and `/odom` rate investigation pending |
| 4 — SLAM | Not started |
| 5 — Nav2 (MVP milestone) | Not started |
| 6–7 — Extended sensors, Semantic perception | Not started |

Phase 3 gate requires: LiDAR verified on `/scan`; EKF `/odom` publishing smoothly; PID velocity error ≤ ±10% at steady state.

---

## 3. System Architecture

| Layer | Hardware | Key Responsibilities |
|---|---|---|
| Embedded controller | ESP32-S3-DevKitC-1 (Lonely Binary expansion) | PID motor control, encoder counting, IMU, cmd_vel watchdog, micro-ROS publisher |
| Sensor bridge | Raspberry Pi 5 | micro-ROS agent, INA219 battery monitor, LiDAR driver, RealSense driver, EKF, light Nav2 nodes |
| High-level compute | Development PC (Ubuntu 24.04) | SLAM Toolbox, Nav2, RViz2, YOLO, rosbag, AI nodes |

---

## 4. Physical Hardware Summary

| Component | Model | Interface |
|---|---|---|
| Microcontroller | ESP32-S3-DevKitC-1 on Lonely Binary expansion | Native USB → Pi `/dev/ttyACM0` (micro-ROS + flash); CH340 UART0 → Pi `/dev/ttyUSB0` (debug console 115200 baud) |
| Motor driver | TB6612FNG — temporary, upgrade planned | GPIO 10–15 |
| Motors + encoders | 4× JGA25-371 DC 12V, 45:1 gear ratio | GPIO 39–42 (PCNT, 1 encoder per side) |
| IMU | Adafruit BNO055 | I2C GPIO 8/9, addr 0x28 |
| Battery monitor | INA219 breakout | Pi I2C-1 GPIO 2/3, addr 0x40 |
| 2D LiDAR | Slamtec RPLidar A1 M8 | USB → Pi `/dev/rplidar` |
| RGB-D camera | Intel RealSense D435 | USB 3.0 → Pi |
| Power | EP-0225 52pi hat | DC barrel 9–24V → 5V/8A USB PD |

Env sensor (BME680) and OLED display (Waveshare 2.42") are not yet wired — Phase 6.

> Full GPIO map, I2C addresses, encoder constants, timing, and TF frame IDs: [`.claude/rules/hardware_constants.md`](.claude/rules/hardware_constants.md)

---

## 5. Power and Safety Architecture

```
Battery (12V 3S LiPo)
    ├── [10A fuse]  →  TB6612FNG VM  (motor power — always live when battery connected)
    └── [3A fuse]   →  Master switch  →  EP-0225
                            ├── USB-C → Pi 5 (5V/5A)
                            │     ├── USB-A → ESP32 ttyACM0  (micro-ROS + flash)
                            │     ├── USB-A → ESP32 ttyUSB0  (debug console)
                            │     ├── USB-A → RPLidar A1
                            │     └── USB-A → RealSense D435
                            └── VIN pad (tied to barrel input — do not double-connect)
```

**Hard electrical rules:**
- All subsystems share one common ground. Missing ground = serial errors, encoder EMI, motor noise.
- Master switch kills logic rail only; motor fuse is always-on protection.
- GPIO 40/41 (left encoder): 100 nF ceramic caps to GND required in signal path — breadboard fix validated; caps remain mandatory.

---

## 6. Validated Findings and Non-Negotiable Decisions

Truths established by hardware testing. Do not revert without a hardware re-validation.

- **Platform:** ROS 2 Jazzy, Raspberry Pi 5 + ESP32-S3 over native USB micro-ROS (`/dev/ttyACM0`, 921600 baud).
- **Drive base:** P-only control validated; straight drive confirmed at 0.10 m/s. KI introduction is next Phase 3 task.
- **Encoder signal path:** Fixed (bad breadboard section identified and corrected). PCNT + `setFilter(400)` validated under motor load. EMA disabled (`VEL_ALPHA = 1.0`) — do not reintroduce EMA; it causes KD phase lag.
- **Encoder CPR:** Unresolved. Firmware uses `990`; historical docs say `1010`. Re-run 10-revolution direct count test (3 trials per wheel) after corrected wiring before standardizing either value.
- **Battery monitoring:** Pi-side INA219 is authoritative. Must be configured `RANGE_32V` + `GAIN_8_320MV` — default gain causes 32.76V / NaN. Fix committed 2026-05-30; verify reading ~9.9–12.6V before trusting low-voltage cutoff.
- **IMU under motor load:** BNO055 `angular_velocity.z` is vibration-contaminated (validated: mean +0.113 rad/s, spikes to ±11.3 rad/s during straight drive). EKF `imu0_config[11]` (vyaw) is **disabled**. Do not use IMU yaw to tune motors or drive EKF yaw fusion. Re-enable only after mechanical isolation and re-validation.
- **EKF `/odom` rate:** Configured 20 Hz; currently observed ~55 Hz externally after time-sync. Root cause under investigation — do not assume 20 Hz is correct until resolved.
- **Time sync:** ESP32 must call `rmw_uros_sync_session()` before stamping odom/IMU messages with epoch time.
- **micro-ROS reconnect:** Fully automatic. Pings agent every 2s; 3 failures → motors stop → entities destroyed → retry every 1s. Recovery ≤7s with no physical RESET required.
- **QoS:** `/diff_cont/odom` and `/imu/imu` publishers are RELIABLE (micro-ROS fragmentation requirement — Odometry is ~712 bytes, exceeds 512-byte XRCE MTU). Subscribers must use RELIABLE. SENSOR_QOS (BEST_EFFORT) silently drops these messages.
- **PWM frequency:** 1 kHz validated on this chassis. 20 kHz caused 10× measured speed loss. Do not change without hardware re-validation.
- **Watchdog:** Firmware currently `WATCHDOG_MS = 2000` (2 seconds). Target is 500 ms. **Open safety mismatch** — do not assume the robot stops within 500 ms after command loss until firmware is updated, flashed, and stop-time validated.
- **CH340 (`/dev/ttyUSB0`):** Debug console only at 115200 baud. Not battery telemetry or display data. Display daemon reads `/battery_state` via ROS 2.

---

## 7. Current Blockers / Next Tests

| Item | Status |
|---|---|
| Encoder CPR recount | **Blocked** — 10-rev direct count test not yet run post-wiring-fix |
| Watchdog 500 ms | **Open** — firmware still 2000 ms; requires firmware change + flash + stop-time validation |
| INA219 reading verification | **Pending** — fix committed 2026-05-30; confirm reading ~9.9–12.6V under load |
| LiDAR on `/scan` | **Pending** — Phase 3 gate; not yet verified |
| EKF `/odom` rate anomaly | **Under investigation** — configured 20 Hz; observed ~55 Hz externally after time-sync |
| Sensor physical offsets in URDF | **Pending** — current positions are placeholders; measure before relying on EKF |

---

## 8. Entry Points

All operations that touch hardware or deploy to the robot must use these scripts.

| Task | Command |
|---|---|
| Build workspace | `cd ~/bot_ws && colcon build --symlink-install` |
| Flash ESP32 | `scripts/flash_esp32.sh` — stops/masks agent, flashes, restores |
| Deploy to Pi | `scripts/deploy.sh` |
| Motor test | `~/motor_test.sh [vx_m_s] [duration_s]` (run on Pi) — bags data automatically |
| Health check | `scripts/health_check.sh` |
| Start micro-ROS agent | `sudo systemctl start microros-agent.service` |
| SSH to Pi | `ssh ubuntu@pi5bot` |

**Never flash with a manual esptool command while `microros-agent.service` is running.** It grabs `/dev/ttyACM0` mid-write. `scripts/flash_esp32.sh` handles stop/mask/restore.

---

## 9. Canonical Reference Files

| Topic | File |
|---|---|
| GPIO map, I2C addresses, encoder constants, timing, TF frame IDs, serial roles | [`.claude/rules/hardware_constants.md`](.claude/rules/hardware_constants.md) |
| ROS topics, QoS profiles, TF tree, transport | [`.claude/rules/ros2_communication.md`](.claude/rules/ros2_communication.md) |
| Motor test safety, data capture, analysis requirements | [`.claude/rules/testing.md`](.claude/rules/testing.md) |
| EKF config, Nav2 layout, motor control, watchdog, display daemon, debug commands | [`.claude/rules/robot_specific.md`](.claude/rules/robot_specific.md) |
| Node architecture patterns, lifecycle nodes, executors | [`.claude/rules/ros2_nodes.md`](.claude/rules/ros2_nodes.md) |
| Clean architecture (domain / application / infrastructure layers) | [`.claude/rules/clean_architecture.md`](.claude/rules/clean_architecture.md) |
| ROS general standards (packages, launch, logging, QoS code patterns) | [`.claude/rules/ros2_general.md`](.claude/rules/ros2_general.md) |
| Step-by-step build plan and phase gate acceptance criteria | [`docs/architecture/build_plan.md`](docs/architecture/build_plan.md) |
| Hardware component references | [`docs/hardware/`](docs/hardware/) |
| Validation run logs and test protocols | [`docs/testing/`](docs/testing/) |

---

## 10. Hard Safety Summary

Full rules in [`.claude/rules/testing.md`](.claude/rules/testing.md). These apply at all times:

1. **Before any motor movement:** ask the user if the robot is in position and they are ready. Wait for an explicit reply. Do not run the command until confirmed.
2. **Never background a cmd_vel publisher.** `run_in_background: true` on a cmd_vel publisher is forbidden — robot hit a wall 2026-05-30 from this pattern.
3. **All motor tests use `motor_test.sh`.** No manual bag + publisher combos. Script handles bag, drive, stop, and cleanup atomically.
4. **Flash only with `scripts/flash_esp32.sh`.** Never use raw esptool while `microros-agent.service` is running.
5. **Before any non-trivial change:** explain what, why, and tradeoff — then stop and wait for explicit confirmation. Do not write code, config, or firmware until the user confirms. Non-trivial = any firmware change, build/flash, script/config edit, or any change with more than one approach.
