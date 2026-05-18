# Autonomous Multi-Sensor Mobile Robot — CLAUDE.md

This file provides AI coding assistants (Claude Code, ChatGPT Codex, etc.) with the context needed to contribute effectively to this project.

Hardware docs live in `docs/hardware/`. The quick-reference GPIO map is in `docs/hardware/README.md`.

---

## Project Overview

A distributed ROS 2 Humble autonomous mobile robot (AMR) — clean, standalone build from scratch. Capable of autonomous mapping, SLAM, navigation, obstacle avoidance, multi-sensor fusion, semantic perception, and environmental monitoring. Architecture mirrors a commercial AMR, not a hobby robot.

> **For AI assistants:** This is a standalone project. Do not reference, import patterns from, or compare against any other robot project. All wiring and pinout data in `docs/hardware/` is correct for this build.

---

## System Architecture

```
Development PC  ←→  Wi-Fi / Ethernet  ←→  Raspberry Pi 5  ←→  USB (micro-ROS)  ←→  ESP32-S3
(Brain)                                    (Nervous System)                          (Reflexes)
SLAM / Nav2 / AI / RViz                    Sensor bridge / ROS drivers              Motion / Safety
```

### Layer Responsibilities

| Layer | Hardware | Key Responsibilities |
|---|---|---|
| Embedded controller | ESP32-S3-DevKitC-1 on expansion base | PID motor control, encoder counting, IMU + battery sensing, safety watchdog, micro-ROS publisher |
| Sensor bridge | Raspberry Pi 5 | micro-ROS agent, LiDAR driver, RealSense driver, EKF, Nav2 (light nodes) |
| High-level compute | Development PC | SLAM Toolbox, Nav2, RViz2, YOLO, rosbag, AI nodes |

---

## Confirmed Hardware

| Component | Model | Interface | Layer |
|---|---|---|---|
| Power | RPI5 PD Power Hat P01 | DC barrel 9–24V → 5V/8A USB PD to Pi | Pi |
| Compute (Pi) | Raspberry Pi 5 | USB PD power, USB-A devices, Ethernet/Wi-Fi | Pi |
| Microcontroller | ESP32-S3-DevKitC-1 on expansion base | Native USB HWCDC → Pi `/dev/ttyACM0` | ESP32 |
| Motor driver | Adafruit TB6612FNG breakout | GPIO 10–15 (PWM + direction) | ESP32 |
| Motors + encoders | JGA25-371 DC 12V, 45:1 gear ratio | GPIO 39–42 (quadrature, 1010 CPR) | ESP32 |
| IMU | Adafruit BNO055 breakout | I2C GPIO 8/9, addr 0x28 | ESP32 |
| Battery monitor | Adafruit INA219 breakout | I2C GPIO 8/9, addr 0x40 | ESP32 |
| Env sensor | BME680 breakout | I2C GPIO 8/9, addr 0x76 — **not yet wired** | ESP32 |
| 2D LiDAR | Slamtec RPLidar A1 M8 | USB 2.0 → Pi `/dev/rplidar` | Pi |
| RGB-D camera | Intel RealSense D435 | USB 3.0 → Pi (640×480 @ 15fps, RSUSB) | Pi |

---

## Power Architecture

```
Battery (9–24V DC, e.g. 3S LiPo ~12V)
    └── RPI5 PD Power Hat INPUT (DC barrel)
            ├── OUTPUT USB-C  →  Raspberry Pi 5 (5.15V / 5A, USB PD 3.0)
            │       ├── Pi USB-A  →  ESP32-S3       (power + micro-ROS serial)
            │       ├── Pi USB-A  →  RPLidar A1      (power + data, USB 2.0)
            │       └── Pi USB-A  →  RealSense D435  (power + data, USB 3.0)
            └── VIN screw terminal  →  TB6612FNG VM  (raw battery voltage, motor power)

TB6612 logic VCC  →  ESP32 3V3 pin
Common ground: Battery −, hat GND, Pi GND, ESP32 GND, TB6612 GND — all one rail.
```

---

## ESP32-S3 GPIO Map (confirmed)

| GPIO | Function |
|---|---|
| 8 | I2C SDA — BNO055 (0x28), INA219 (0x40), BME680 (0x76 planned) |
| 9 | I2C SCL |
| 10 | PWMA — Right motor speed (LEDC ch 0, 1 kHz, 8-bit) |
| 11 | AIN1 — Right motor direction A |
| 12 | AIN2 — Right motor direction B |
| 13 | PWMB — Left motor speed (LEDC ch 1, 1 kHz, 8-bit) |
| 14 | BIN1 — Left motor direction A |
| 15 | BIN2 — Left motor direction B |
| 19, 20 | Native USB D−/D+ — micro-ROS HWCDC transport to Pi |
| 39 | Right encoder B (read in ISR) |
| 40 | Left encoder A — `attachInterrupt` CHANGE ⚠️ EMI |
| 41 | Left encoder B (read in ISR) ⚠️ EMI |
| 42 | Right encoder A — `attachInterrupt` CHANGE |

**Motor A = RIGHT, Motor B = LEFT.**
STBY not wired — Adafruit breakout has onboard 10 kΩ pull-up (always enabled).

⚠️ **GPIO 40/41 EMI:** Left encoder picks up TB6612 1 kHz PWM noise. EMA filter (`VEL_ALPHA = 0.2`) mitigates in firmware. Permanent fix: 100 nF ceramic caps, GPIO 40 → GND and GPIO 41 → GND at ESP32 headers.

**Avoid:** GPIO 4,5,6,7 (not broken out), 19/20 (USB), 25,26,27,32,33 (not broken out), 35/36/37 (internal flash), 38 (RGB LED), 43/44 (UART0), 0/45/46 (strapping pins).

---

## Encoder Constants (validated)

| Parameter | Value | Notes |
|---|---|---|
| `ENC_CPR` | 1010 | 2× quadrature, 45:1 gear ratio — validated on floor |
| `wheel_radius` | 0.034 m | Measured (68 mm dia; datasheet says 65 mm) |
| `wheel_separation` | 0.179 m | Measured center-to-center |

Encoder wire colors (JGA25-371): Red/White = motor power, Blue/Black = encoder power, Yellow = Ch A, Green = Ch B.

---

## micro-ROS Transport (ESP32 ↔ Pi)

ESP32-S3 native USB HWCDC → USB cable → Pi `/dev/ttyACM0`

Stable by-id path: `/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:5C:23:1C-if00`

Required firmware build flag: `-DARDUINO_USB_CDC_ON_BOOT=1`

Wi-Fi used only for OTA flashing and TelnetStream debug monitoring — **not** for micro-ROS.

Run agent on Pi:
```bash
source /opt/ros/humble/setup.bash
source ~/microros_ws/install/setup.bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0
```

---

## ROS 2 Topic Architecture

| Topic | Publisher | Consumer | Rate |
|---|---|---|---|
| `/diff_cont/cmd_vel_unstamped` | Nav2 / twist_mux | ESP32 (via micro-ROS) | 20 Hz |
| `/diff_cont/odom` | ESP32 micro-ROS | robot_localization EKF | 30 Hz |
| `/imu/imu` | ESP32 micro-ROS | robot_localization EKF | 30 Hz |
| `/battery_state` | ESP32 micro-ROS | monitoring nodes | 1 Hz |
| `/odom` | robot_localization | Nav2, SLAM | 20 Hz |
| `/scan` | rplidar_node | slam_toolbox, Nav2 obstacle layer | ~5.5 Hz |
| `/camera/depth/points` | realsense2_camera | Nav2 voxel layer | 15 Hz |
| `/camera/color/image_raw` | realsense2_camera | future: YOLO | 15 Hz |

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

`map → odom → base_link` is the foundation. Do not break this chain.

---

## Folder Structure

**Always follow this layout. Do not create files outside these locations without a documented reason.**

```
dev1_ws/                              ← workspace root (this repo)
├── src/                              ← all ROS 2 packages (colcon builds from here)
│   ├── esp32_serial_bridge/          ← Python package: micro-ROS agent bridge node
│   │   ├── esp32_serial_bridge/      ← Python module (node source)
│   │   ├── launch/
│   │   ├── config/
│   │   ├── resource/                 ← ament_python marker (do not edit)
│   │   ├── test/
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
│   └── esp32/                        ← ESP32-S3 firmware (PlatformIO/Arduino, not a ROS pkg)
│       ├── src/                      ← .cpp / .ino source files
│       ├── include/                  ← .h header files
│       └── lib/                      ← local libraries
├── docs/
│   ├── hardware/                     ← per-component hardware reference docs
│   │   ├── README.md                 ← hardware index + GPIO quick-reference
│   │   ├── rpi5_pd_power_hat.md
│   │   ├── dfr0205.md
│   │   ├── esp32_s3.md
│   │   ├── raspberry_pi_5.md
│   │   ├── development_pc.md
│   │   ├── tb6612fng.md
│   │   ├── wheel_encoders.md
│   │   ├── rplidar.md
│   │   ├── realsense_d435.md
│   │   ├── bno055_imu.md
│   │   ├── ina219_battery_monitor.md
│   │   ├── bme680_environmental.md
│   │   └── wiring_audit.md
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
| ESP32 firmware source | `firmware/esp32/src/` |
| Hardware reference doc | `docs/hardware/` |
| Architecture / design doc | `docs/architecture/` |
| Utility shell script | `scripts/` |

---

## Timing Requirements

| System | Rate |
|---|---|
| PID / encoder loop | 100 Hz |
| IMU + odom publish (micro-ROS) | 30 Hz |
| LiDAR scan (RPLidar A1) | ~5.5 Hz |
| RealSense depth + color | 15 Hz |
| EKF output (`/odom`) | 20 Hz |
| Nav2 controller | 20 Hz |
| Battery publish | 1 Hz |

---

## Critical Design Rules

### Safety
- ESP32 safety watchdog must run continuously, independent of ROS, Wi-Fi, and the dev PC.
- If no velocity command is received within the timeout window, ESP32 stops the motors immediately.
- Battery voltage cutoff runs on the ESP32 — never depend on ROS for battery safety.
- **Dev PC failure must never cause a dangerous robot.**

### Electrical
- All subsystems share one common ground: Battery −, ESP32, Pi, TB6612, encoders, sensors.
- Missing common ground causes serial errors, PWM noise, encoder EMI, and motor glitches.
- GPIO 40/41 (left encoder) require 100 nF ceramic caps to GND — TB6612 1 kHz PWM couples into these lines.

### Motion Control
- Closed-loop PID velocity control only (encoder feedback → wheel velocity target in rad/s).
- Never use open-loop PWM for normal operation.
- Motor A = RIGHT wheel, Motor B = LEFT wheel — do not swap.

### micro-ROS
- Transport is native USB HWCDC, not Wi-Fi. Build flag `-DARDUINO_USB_CDC_ON_BOOT=1` is required.
- Never change serial device, encoder pins, motor polarity, or controller YAML all at once during debugging. Change one thing, observe, repeat.
- If `micro_ros_agent` gets stuck after OTA flash or watchdog reset: `sudo systemctl restart robot-launch.service`.

### SLAM / Costmaps
- LiDAR is mounted low — detects chair legs and shoes as walls. Mitigate with RealSense depth validation and layered costmaps.
- Nav2 costmap layers: Static (SLAM map) → Obstacle (LiDAR) → Voxel (RealSense) → Semantic (YOLO, future).

### URDF
- All sensor frames defined in URDF relative to `base_link`. TF must always match physical sensor placement.
- Never rename ROS controller, joint, plugin, or topic identifiers without updating all dependents.

---

## SLAM Stack

| Phase | Tool | Reason |
|---|---|---|
| Current | slam_toolbox | Stable 2D mapping, native Nav2 integration |
| Future | RTAB-Map | RGB-D fusion, visual loop closure, semantic mapping |

Sensor fusion layers:
1. **Motion fusion** — encoders + BNO055 → `/odom` via `robot_localization` EKF
2. **Mapping fusion** — LiDAR geometry + RealSense depth → wall validation
3. **Semantic fusion** (future) — YOLO on dev PC GPU → obstacle classification

EKF config notes: IMU orientation is **disabled** (magnetometer unreliable on metal chassis). Angular velocity and linear acceleration are enabled.

---

## Development Order

1. Reliable motor control (PID + encoders confirmed working via micro-ROS)
2. Stable odometry (`robot_localization` EKF tuned, `/odom` smooth)
3. Reliable SLAM (clean map with `slam_toolbox`)
4. Correct TF tree (URDF + static transforms validated in RViz2)
5. Nav2 integration (autonomous point-to-point navigation)
6. RealSense fusion (voxel costmap layer)
7. BME680 environmental sensor (add to I2C bus, publish env data)
8. Semantic perception (YOLO on dev PC GPU)

---

## Useful Debugging Commands

```bash
# Verify devices on Pi
ls /dev/rplidar
ls /dev/serial/by-id/usb-Espressif*

# micro-ROS agent
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0

# Topic health
ros2 topic hz /diff_cont/odom
ros2 topic hz /imu/imu
ros2 topic hz /scan
ros2 topic echo /battery_state

# TF debugging
ros2 run tf2_tools view_frames
ros2 run tf2_ros tf2_echo base_link laser

# I2C sensor check (on ESP32 host via Pi)
sudo i2cdetect -y 1   # expect: 0x28 (BNO055), 0x40 (INA219)

# LiDAR stale process fix
sudo fuser -k /dev/rplidar
```

---

## MVP Acceptance Criteria

The robot is considered minimally operational when it can:

- Drive reliably under closed-loop PID velocity control
- Publish stable `/diff_cont/odom` and `/odom` (EKF fused)
- Build a consistent 2D map with `slam_toolbox`
- Avoid collisions during autonomous Nav2 navigation
- Stop safely on ESP32 watchdog timeout (no Pi or PC required)
- Stream battery state at 1 Hz independently of ROS

---

## Environment

| Item | Value |
|---|---|
| OS | Ubuntu 22.04 LTS |
| ROS | ROS 2 Humble Hawksbill |
| Pi | Raspberry Pi 5 |
| ESP32 firmware | PlatformIO + Arduino framework — `firmware/esp32/` |
| micro-ROS agent | Built from source in `~/microros_ws` (not in apt for arm64) |
| Python | 3.10+ for ROS nodes |
| Dev PC GPU | NVIDIA RTX preferred for YOLO / point cloud processing |
