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
| 1 — ESP32 Firmware | **Functional baseline complete** — P-only drive, encoders, IMU, reconnect, and watchdog 500 ms validated |
| 2 — ROS 2 Foundation (URDF + TF) | **Complete** — 9 frames, all named correctly, `base_footprint` root added |
| 3 — Sensor Bridge & EKF | **In progress** — drivetrain/safety checks passed; EKF /odom rate issue open |
| 4 — SLAM | Not started |
| 5 — Nav2 (MVP milestone) | Not started |
| 6–7 — Extended sensors, Semantic perception | Not started |

Phase 3 gate requires: LiDAR verified on `/scan`; EKF `/odom` publishing smoothly; PID velocity error ≤ ±10% at steady state.

---

## 3. System Architecture

| Layer | Hardware | Key Responsibilities |
|---|---|---|
| Embedded controller | ESP32-S3-DevKitC-1 (Lonely Binary expansion) | PID motor control, encoder counting, IMU, cmd_vel watchdog, micro-ROS publisher |
| Sensor bridge | Raspberry Pi 5 | micro-ROS agent, INA219 battery monitor, LiDAR driver, RealSense driver, EKF |
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
- **Encoder CPR:** **Validated 2026-05-31 — 1010 counts per wheel revolution.** Measured via `/diagnostics/encoder_counts` topic, 3 × 1.500 m straight push test (6.9938 rev per trial). Trials 2+3 accepted: L 1010.5/1008.9, R 1009.2/1009.2. Trial 1 right (1020.3) excluded as directional push outlier. Historical documentation value confirmed. Firmware constant `ENC_CPR_F = 1010.0f` — do not revert.
- **Battery monitoring:** Pi-side INA219 is authoritative. Must be configured `RANGE_32V` + `GAIN_8_320MV` — default gain causes 32.76V / NaN. Fix committed 2026-05-30; verify reading ~9.9–12.6V before trusting low-voltage cutoff. **Verified 2026-05-31** — RANGE_32V + GAIN_8_320MV confirmed in source; voltage readings 12.0–12.3 V across session, consistent with 3S LiPo; INA219 measures logic-rail current only (EP-0225 branch), not motor current — motor current flows through the separate 10A fuse path and is not visible to the INA219.
- **IMU under motor load:** BNO055 `angular_velocity.z` is vibration-contaminated (validated: mean +0.113 rad/s, spikes to ±11.3 rad/s during straight drive). EKF `imu0_config[11]` (vyaw) is **disabled**. Do not use IMU yaw to tune motors or drive EKF yaw fusion. Re-enable only after mechanical isolation and re-validation.
- **EKF `/odom` rate:** Configured 20 Hz; observed ~8 Hz externally (root cause identified: robot_localization suppresses publish when `getLastMeasurementTime()` has not advanced past `last_published_stamp_`; probable cause is ESP32 timestamp offset queuing measurements; under investigation — do not assume 20 Hz is correct until resolved).
- **Time sync:** ESP32 must call `rmw_uros_sync_session()` before stamping odom/IMU messages with epoch time.
- **micro-ROS reconnect:** Fully automatic. Pings agent every 2s; 3 failures → motors stop → entities destroyed → retry every 1s. Recovery ≤7s with no physical RESET required.
- **QoS:** `/diff_cont/odom` and `/imu/imu` publishers are RELIABLE (micro-ROS fragmentation requirement — Odometry is ~712 bytes, exceeds 512-byte XRCE MTU). Subscribers must use RELIABLE. SENSOR_QOS (BEST_EFFORT) silently drops these messages.
- **PWM frequency:** 1 kHz validated on this chassis. 20 kHz caused 10× measured speed loss. Do not change without hardware re-validation.
- **Watchdog:** Source changed to `WATCHDOG_MS = 500` ms. **Validated 2026-05-31** — firmware flashed; measured stop time 0.538 s after command loss (criterion ≤ 0.600 s, PASSED).
- **CH340 (`/dev/ttyUSB0`):** Debug console only at 115200 baud. Not battery telemetry or display data. Display daemon reads the Pi-side INA219 directly over I2C.
- **LiDAR `/scan`:** Verified publishing on the RPLidar A1 M8. Software motor shutdown via DTR/RTS toggling was tested and failed on this adapter — the motor does not respond to serial control signals. Physical motor-off requires future switched USB power hardware; do not attempt software motor control on this device.

---

## 7. Current Blockers / Next Tests

| Item | Status |
|---|---|
| Encoder CPR recount | **Closed 2026-05-31** — CPR = 1010 confirmed via `/diagnostics/encoder_counts` topic; CH340 bypass not required |
| CH340 debug-output failure | **Root cause identified** — DTR-assertion burst only; `in_waiting` returns 0 permanently after burst. Not blocking any active work — CPR resolved via micro-ROS topic instead. No further action required unless CH340 debug stream is needed for future diagnostics. |
| Odometry validation (CPR=1010) | **Passed 2026-05-31** — motor_test.sh 0.10 m/s 8 s: velocity error −6.1%, distance error −8.3%, both ≤±10% gate. Errors larger than the CPR=990 gate run (−0.9% / −2.7%) due to lower battery voltage at time of test (11.67 V vs 12.07 V) — P-only KP=0.25 has no integral correction for supply voltage variation. Odom now reports accurate physical distance. |
| Watchdog 500 ms | **Validated 2026-05-31** — 0.538 s stop after command loss; criterion ≤ 0.600 s PASSED |
| INA219 reading verification | **Verified 2026-05-31** — voltage-based cutoff confirmed; current reading is logic rail only, not motor current |
| PID velocity gate (≤ ±10% error) | **Passed 2026-05-31** — steady-state error −0.9% at 0.10 m/s (P-only, KP=0.25); distance error −2.7%; encoder yaw drift −0.015 rad/s mean |
| EKF `/odom` rate anomaly | **Under investigation** — configured 20 Hz; observed ~8 Hz (not ~55 Hz as previously noted); root cause mechanism identified in robot_localization source; timestamp offset hypothesis to be tested |
| LiDAR on `/scan` | **Verified** — publishing confirmed; software motor-off via DTR/RTS failed on this adapter; physical cutoff requires switched USB power (future hardware) |
| Sensor physical offsets in URDF | **Pending** — current positions are placeholders; measure before Phase 4 SLAM-quality validation; do not block Phase 3 service/rate validation |

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
