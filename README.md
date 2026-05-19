# MyBot1 — Autonomous Multi-Sensor Mobile Robot

A distributed ROS 2 autonomous mobile robot (AMR) built on a proven hardware stack. Capable of autonomous mapping, SLAM, navigation, obstacle avoidance, multi-sensor fusion, and environmental monitoring.

---

## System Overview

![System Architecture](docs/architecture/system_overview.png)

---

## Hardware

| Component | Model |
|---|---|
| Power | RPI5 PD Power Hat P01 (9–24V in → 5V/8A USB PD) |
| Compute | Raspberry Pi 5 |
| Microcontroller | ESP32-S3-DevKitC-1 on Lonely Binary expansion board |
| Motor driver | Adafruit TB6612FNG |
| Motors + encoders | JGA25-371 DC 12V, 45:1, 1010 CPR |
| IMU | Adafruit BNO055 (I2C 0x28) |
| Battery monitor | Adafruit INA219 (I2C 0x40) |
| Environmental | BME680 (I2C 0x76) — planned |
| 2D LiDAR | Slamtec RPLidar A1 M8 |
| RGB-D camera | Intel RealSense D435 |

---

## Software Stack

| Component | Version |
|---|---|
| OS | Ubuntu 22.04 LTS |
| ROS | ROS 2 Humble Hawksbill |
| SLAM | slam_toolbox (current) / RTAB-Map (future) |
| Navigation | Nav2 |
| Localization | robot_localization (EKF) |
| ESP32 firmware | PlatformIO + Arduino + micro-ROS |
| Visualization | RViz2 |

---

## Repository Structure

```
bot_ws/
├── src/
│   ├── esp32_serial_bridge/    # micro-ROS agent bridge node (Python)
│   ├── robot_bringup/          # top-level launch files
│   ├── robot_description/      # URDF, meshes, TF
│   ├── robot_navigation/       # Nav2 config, costmap params, maps
│   ├── robot_slam/             # SLAM Toolbox / RTAB-Map config
│   └── robot_msgs/             # custom message definitions
├── firmware/
│   └── esp32/                  # ESP32-S3 firmware (PlatformIO)
├── docs/
│   ├── hardware/               # per-component hardware + wiring docs
│   ├── architecture/           # system architecture docs
│   └── testing/                # test protocols, validation logs, audit reports
└── scripts/                    # utility shell scripts
```

---

## ESP32-S3 GPIO Map

| GPIO | Function |
|---|---|
| 8 / 9 | I2C SDA / SCL (BNO055, INA219, BME680) |
| 10 | PWMA — Right motor speed |
| 11 / 12 | AIN1 / AIN2 — Right motor direction |
| 13 | PWMB — Left motor speed |
| 14 / 15 | BIN1 / BIN2 — Left motor direction |
| 19 / 20 | Native USB (micro-ROS to Pi) |
| 40 / 41 | Left encoder A / B |
| 42 / 39 | Right encoder A / B |

---

## Key ROS 2 Topics

| Topic | Direction | Rate |
|---|---|---|
| `/diff_cont/cmd_vel_unstamped` | Dev PC → ESP32 | 20 Hz |
| `/diff_cont/odom` | ESP32 → Pi | 30 Hz |
| `/imu/imu` | ESP32 → Pi | 30 Hz |
| `/battery_state` | ESP32 → Pi | 1 Hz |
| `/odom` | EKF fused output | 20 Hz |
| `/scan` | RPLidar → SLAM | ~5.5 Hz |
| `/camera/depth/points` | RealSense → Nav2 | 15 Hz |

---

## Hardware Docs

Full pinouts, wiring, and notes for every component: [`docs/hardware/`](docs/hardware/README.md)

Known wiring issues and improvement suggestions: [`docs/hardware/wiring_audit.md`](docs/hardware/wiring_audit.md)

---

## Testing Docs

Gate-style test protocols, validation run logs, and audit reports: [`docs/testing/`](docs/testing/)

Files follow the naming convention `<type>_<YYYY-MM-DD>.md`. Protocols describe how to test; logs record what happened during a run.

---

## Development Order

1. Reliable motor control (PID + encoders via micro-ROS)
2. Stable odometry (EKF tuned)
3. Reliable SLAM (clean map)
4. Correct TF tree (validated in RViz2)
5. Nav2 autonomous navigation
6. RealSense voxel costmap fusion
7. BME680 environmental sensing
8. Semantic perception (YOLO)

---

## For AI Assistants

See [`CLAUDE.md`](CLAUDE.md) for full project context, GPIO map, topic architecture, design rules, and folder conventions.
