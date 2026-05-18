# Autonomous Multi-Sensor Mobile Robot — CLAUDE.md

This file provides AI coding assistants (Claude Code, ChatGPT Codex, etc.) with the context needed to contribute effectively to this project.

---

## Project Overview

A distributed ROS 2 Autonomous Mobile Robot (AMR) capable of autonomous mapping, navigation, multi-sensor SLAM, obstacle avoidance, semantic perception, and environmental monitoring. The architecture deliberately mirrors a commercial AMR rather than a hobby robot.

---

## System Architecture

Three-layer distributed compute model:

```
Development PC  ←→  (Wi-Fi / Ethernet)  ←→  Raspberry Pi  ←→  (USB Serial)  ←→  ESP32-S3
(Brain)                                      (Nervous System)                  (Reflexes)
SLAM / Nav2 / AI / RViz                      Sensor bridge / ROS drivers       Motion / Safety
```

### Layer Responsibilities

| Layer | Hardware | Key Responsibilities |
|---|---|---|
| Embedded controller | ESP32-S3 | PID motor control, encoder counting, IMU/battery/env sensing, safety watchdog |
| Sensor bridge | Raspberry Pi 5 | Serial bridge to ESP32, LiDAR driver, RealSense driver, local ROS nodes |
| High-level compute | Development PC | SLAM Toolbox, Nav2, RViz, YOLO, rosbag, AI nodes |

---

## Hardware Components

| Component | Interface | Layer |
|---|---|---|
| TB6612FNG motor driver | ESP32 GPIO/PWM | ESP32 |
| Wheel encoders | ESP32 hardware interrupts | ESP32 |
| BNO055 IMU | ESP32 I2C | ESP32 |
| INA219 battery monitor | ESP32 I2C | ESP32 |
| BME680 env sensor | ESP32 I2C | ESP32 |
| RPLIDAR (2D LiDAR) | Pi USB | Pi |
| RealSense D435 (RGB-D) | Pi USB | Pi |

---

## Serial Protocol (Pi ↔ ESP32)

### Pi → ESP32 Commands

```
V <left_rad_s> <right_rad_s>    # Set wheel velocities
STOP                             # Immediate stop
PING                             # Keepalive
```

### ESP32 → Pi Telemetry

```
ENC <left_ticks> <right_ticks>
VEL <left_rad_s> <right_rad_s>
IMU <roll> <pitch> <yaw>
BAT <voltage> <current> <power>
ENV <temp> <humidity> <pressure> <gas_resistance>
ERR <error_code>
```

---

## ROS 2 Topic Architecture

| Topic | Publisher | Consumer |
|---|---|---|
| `/cmd_vel` | Nav2 | serial bridge |
| `/odom` | robot_localization | Nav2, SLAM |
| `/scan` | RPLIDAR driver | slam_toolbox |
| `/camera/depth/points` | RealSense driver | costmap voxel layer |
| `/imu` | ESP32 serial bridge | robot_localization |
| `/battery_state` | ESP32 serial bridge | monitoring nodes |

---

## TF2 Frame Tree

```
map
 └── odom
      └── base_link
           ├── laser
           ├── imu_link
           ├── camera_link
           │     └── camera_depth_frame
           ├── left_wheel
           └── right_wheel
```

The `map → odom → base_link` chain is the foundation of all localization. Do not break this chain.

---

## Folder Structure

**Always follow this layout. Do not create files outside of these locations without a documented reason.**

```
dev1_ws/                              ← workspace root (this repo)
├── src/                              ← all ROS 2 packages (colcon builds from here)
│   ├── esp32_serial_bridge/          ← Python package: USB serial ↔ ROS bridge
│   │   ├── esp32_serial_bridge/      ← Python module (node source)
│   │   ├── launch/                   ← package-level launch files
│   │   ├── config/                   ← package-level YAML config
│   │   ├── resource/                 ← ament_python marker (do not edit)
│   │   ├── test/                     ← unit tests
│   │   ├── package.xml
│   │   ├── setup.py
│   │   └── setup.cfg
│   ├── robot_bringup/                ← CMake package: top-level launch files
│   │   ├── launch/
│   │   ├── config/
│   │   ├── package.xml
│   │   └── CMakeLists.txt
│   ├── robot_description/            ← CMake package: URDF + meshes + TF
│   │   ├── urdf/
│   │   ├── meshes/
│   │   ├── launch/
│   │   ├── config/
│   │   ├── package.xml
│   │   └── CMakeLists.txt
│   ├── robot_navigation/             ← CMake package: Nav2 config + maps
│   │   ├── launch/
│   │   ├── config/
│   │   ├── maps/
│   │   ├── package.xml
│   │   └── CMakeLists.txt
│   ├── robot_slam/                   ← CMake package: SLAM Toolbox / RTAB-Map config
│   │   ├── launch/
│   │   ├── config/
│   │   ├── package.xml
│   │   └── CMakeLists.txt
│   └── robot_msgs/                   ← CMake package: custom msg/srv/action definitions
│       ├── msg/
│       ├── srv/
│       ├── action/
│       ├── package.xml
│       └── CMakeLists.txt
├── firmware/
│   └── esp32/                        ← ESP32-S3 firmware (Arduino/PlatformIO, not a ROS pkg)
│       ├── src/                      ← .cpp / .ino source files
│       ├── include/                  ← .h header files
│       └── lib/                      ← local libraries
├── docs/
│   ├── hardware/                     ← per-component hardware reference docs
│   │   ├── README.md                 ← hardware index
│   │   ├── esp32_s3.md
│   │   ├── raspberry_pi_5.md
│   │   ├── development_pc.md
│   │   ├── tb6612fng.md
│   │   ├── wheel_encoders.md
│   │   ├── rplidar.md
│   │   ├── realsense_d435.md
│   │   ├── bno055_imu.md
│   │   ├── ina219_battery_monitor.md
│   │   └── bme680_environmental.md
│   └── architecture/
│       └── system_overview.md
├── scripts/                          ← shell utility scripts (not ROS nodes)
├── .gitignore
└── CLAUDE.md                         ← this file

# colcon generates these at build time — never commit them:
# build/   install/   log/
```

### Rules for new files

| What you're adding | Where it goes |
|---|---|
| New ROS node (Python) | New package under `src/`, or inside existing package's Python module folder |
| New launch file | `src/<package>/launch/` |
| New YAML config | `src/<package>/config/` |
| New custom message | `src/robot_msgs/msg/` |
| New custom service | `src/robot_msgs/srv/` |
| URDF / xacro | `src/robot_description/urdf/` |
| Mesh files | `src/robot_description/meshes/` |
| ESP32 firmware | `firmware/esp32/src/` |
| Hardware reference doc | `docs/hardware/` |
| Architecture / design doc | `docs/architecture/` |
| Utility shell script | `scripts/` |

---

## Timing Requirements

| System | Target Rate |
|---|---|
| PID / encoder loop | 100 Hz |
| IMU telemetry | 50 Hz |
| LiDAR scan | 10 Hz |
| RealSense | 30 FPS |
| Nav2 controller | 20 Hz |
| Battery / env polling | 1–5 Hz |

---

## Critical Design Rules

### Safety
- The ESP32 safety watchdog must run continuously and independently of ROS, Wi-Fi, and the development PC.
- If no velocity command is received within the watchdog timeout, the ESP32 must stop the motors.
- Battery cutoff logic lives on the ESP32 — never depend on ROS for battery safety.
- **Dev PC failure must never cause a dangerous robot.**

### Electrical
- All subsystems (ESP32, Pi, TB6612, sensors) must share a common ground. Missing common ground causes serial errors, PWM noise, and motor glitches.

### Motion Control
- The robot uses **closed-loop PID velocity control** (encoder feedback → velocity target). Never use open-loop PWM power commands for normal operation.

### Serial Communication
- Early development uses simple ASCII USB serial (no micro-ROS / DDS / XRCE-DDS). Keep the protocol simple and human-readable during bringup.

### SLAM / Costmaps
- The LiDAR is mounted low; it will detect chair legs and shoes as walls. Mitigate with RealSense depth validation and layered costmaps.
- Nav2 costmap layers: Static (SLAM map) → Obstacle (LiDAR) → Voxel (RealSense) → Semantic (YOLO, future).

### URDF
- All sensor frames must be defined in the URDF relative to `base_link`. TF must always match physical sensor placement.

---

## SLAM Stack

| Phase | Tool | Reason |
|---|---|---|
| Current | slam_toolbox | Stable 2D mapping, native Nav2 integration |
| Future | RTAB-Map | RGB-D fusion, visual loop closure, semantic mapping |

Sensor fusion layers:
1. **Motion fusion** — encoders + BNO055 → `/odom` via `robot_localization`
2. **Mapping fusion** — LiDAR + RealSense depth → wall validation
3. **Semantic fusion** (future) — YOLO → obstacle classification

---

## Development Order

Follow this order to avoid building on unstable foundations:

1. Reliable motor control (PID + encoders working)
2. Stable odometry (robot_localization tuned)
3. Reliable SLAM (clean map generation)
4. Correct TF tree (URDF + static transforms validated in RViz)
5. Nav2 integration (autonomous point-to-point navigation)
6. RealSense fusion (voxel costmap layer)
7. Semantic perception (YOLO integration)

---

## Useful Debugging Commands

```bash
# View full TF tree
ros2 run tf2_tools view_frames

# Check a specific transform
ros2 run tf2_ros tf2_echo base_link laser

# Monitor topics
ros2 topic hz /scan
ros2 topic echo /odom

# Check node graph
ros2 node list
ros2 topic list
```

---

## MVP Acceptance Criteria

The robot is considered minimally operational when it can:

- Drive reliably under closed-loop velocity control
- Publish stable `/odom`
- Build a consistent 2D map with slam_toolbox
- Avoid collisions during autonomous navigation
- Stop safely on watchdog timeout (no Pi/PC required)
- Stream all telemetry (encoders, IMU, battery, environment)

---

## Environment

- **OS:** Ubuntu 22.04 LTS
- **ROS:** ROS 2 Humble
- **Raspberry Pi:** Raspberry Pi 5 (8 GB recommended)
- **ESP32 framework:** Arduino (via PlatformIO) — firmware in `firmware/esp32/`
- **Python:** 3.10+ for ROS nodes
- **GPU (Dev PC):** NVIDIA RTX preferred for YOLO / point cloud processing
