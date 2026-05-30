# Autonomous Multi-Sensor Mobile Robot — CLAUDE.md

This file provides AI coding assistants (Claude Code, ChatGPT Codex, etc.) with the context needed to contribute effectively to this project.

Hardware docs live in `docs/hardware/`. The quick-reference GPIO map is in `docs/hardware/README.md`.

---

## Project Overview

A distributed ROS 2 Jazzy autonomous mobile robot (AMR) — clean, standalone build from scratch. Capable of autonomous mapping, SLAM, navigation, obstacle avoidance, multi-sensor fusion, semantic perception, and environmental monitoring. Architecture mirrors a commercial AMR, not a hobby robot.

> **For AI assistants:** This is a standalone project. Do not reference, import patterns from, or compare against any other robot project. All wiring and pinout data in `docs/hardware/` is correct for this build.

---

## System Architecture

![System Architecture](docs/architecture/system_overview.png)

### Layer Responsibilities

| Layer | Hardware | Key Responsibilities |
|---|---|---|
| Embedded controller | ESP32-S3-DevKitC-1 on Lonely Binary expansion board | PID motor control, encoder counting, IMU, cmd_vel watchdog, micro-ROS publisher |
| Sensor bridge | Raspberry Pi 5 | micro-ROS agent, INA219 battery monitor, LiDAR driver, RealSense driver, EKF, Nav2 (light nodes) |
| High-level compute | Development PC | SLAM Toolbox, Nav2, RViz2, YOLO, rosbag, AI nodes |

---

## Confirmed Hardware

| Component | Model | Interface | Layer |
|---|---|---|---|
| Power | EP-0225 (52pi) | DC barrel 9–24V → 5V/8A USB PD to Pi | Pi |
| Compute (Pi) | Raspberry Pi 5 | USB PD power, USB-A devices, Ethernet/Wi-Fi | Pi |
| Microcontroller | ESP32-S3-DevKitC-1 on Lonely Binary expansion board | Native USB CDC (GPIO 19/20) → Pi `/dev/ttyACM0` (micro-ROS + flashing) \| CH340 UART0 (GPIO 43/44) → Pi `/dev/ttyUSB0` (display telemetry, Phase 6) | ESP32 |
| Motor driver | TB6612FNG (dual channel) — **temporary; will upgrade to larger driver + 2 more wheels** | GPIO 10–15 (PWMA, AIN1, AIN2, PWMB, BIN1, BIN2) | ESP32 |
| Motors + encoders | 4× JGA25-371 DC 12V, 45:1 gear ratio (skid steer, 2 per side) | GPIO 39–42 (quadrature, 1010 CPR, one encoder per side) | ESP32 |
| IMU | Adafruit BNO055 breakout | I2C GPIO 8/9, addr 0x28 | ESP32 |
| Battery monitor | Generic INA219 breakout (6-pin + screw terminals) | Pi I2C-1 (GPIO 2/3), addr 0x40 — series on logic rail (after 3A fuse, before EP-0225) | Pi |
| Env sensor | BME680 breakout | I2C GPIO 8/9, addr 0x76 — **not yet wired** | ESP32 |
| 2D LiDAR | Slamtec RPLidar A1 M8 | USB 2.0 → Pi `/dev/rplidar` | Pi |
| RGB-D camera | Intel RealSense D435 | USB 3.0 → Pi (640×480 @ 15fps, RSUSB) | Pi |
| OLED display | Waveshare 2.42" OLED (SSD1309) | SPI0 GPIO 10/11/8/25/27 → Pi — **not yet wired** | Pi |

---

## Power Architecture

```
Battery (9–24V DC, e.g. 3S LiPo ~12V)
    ├── [10A inline fuse]  →  TB6612FNG VM  (raw battery voltage, motor power)
    └── [3A inline fuse]  →  Master power switch
              └── EP-0225 (52pi) INPUT (DC barrel)
                      ├── OUTPUT USB-C  →  Raspberry Pi 5 (5.15V / 5A, USB PD 3.0)
                      │       ├── Pi USB-A  →  ESP32-S3 native USB CDC        (micro-ROS + flashing, /dev/ttyACM0)
                      │       ├── Pi USB-A  →  ESP32-S3 CH340 UART0           (display telemetry Phase 6, /dev/ttyUSB0)
                      │       ├── Pi USB-A  →  RPLidar A1      (power + data, USB 2.0)
                      │       └── Pi USB-A  →  RealSense D435  (power + data, USB 3.0)
                      └── VIN screw terminal  →  (tied to barrel jack input — do not double-connect)

TB6612FNG VCC (logic)  →  ESP32 3V3 pin
Common ground: Battery −, EP-0225 GND, Pi GND, ESP32 GND, TB6612FNG GND — all one rail.
```

**Fuse sizing rationale:**
- Logic rail (EP-0225 input): 3A — Pi 5 draws up to 5A at 5V ≈ 2.1A at 12V; 3A gives margin without masking real faults
- Motor rail (TB6612FNG VM): 10A — 4× JGA25-371 rated 750 mA each (stall); 10A covers stall condition

**Master power switch:** Inline between battery and EP-0225 barrel jack. Kills logic rail (Pi + ESP32) without disturbing motor rail fuse. Safe to work on robot with switch off; motor fuse remains in place as always-on protection.

**EP-0225 VIN screw terminal:** Tied directly to the barrel jack input — do not run a second battery connection to the screw terminal. Use barrel jack for all input; VIN pad is for board-level access only.

---

## ESP32-S3 GPIO Map (confirmed)

| GPIO | Function |
|---|---|
| 8 | I2C SDA — BNO055 (0x28), BME680 (0x76 planned) |
| 9 | I2C SCL |
| 10 | PWMA — Right side speed (LEDC ch 0, 1 kHz, 8-bit) → TB6612FNG PWMA |
| 11 | AIN1 — Right side direction 1 → TB6612FNG AIN1 |
| 12 | AIN2 — Right side direction 2 → TB6612FNG AIN2 |
| 13 | PWMB — Left side speed (LEDC ch 1, 1 kHz, 8-bit) → TB6612FNG PWMB |
| 14 | BIN1 — Left side direction 1 → TB6612FNG BIN1 |
| 15 | BIN2 — Left side direction 2 → TB6612FNG BIN2 |
| 17 | (free — UART1 not used for micro-ROS; CH340 connects to UART0 GPIO 43/44) |
| 18 | (free) |
| 19, 20 | Native USB D−/D+ — micro-ROS transport + flashing → Pi `/dev/ttyACM0` |
| 39 | Right encoder B (PCNT) |
| 40 | Left encoder A (PCNT) ⚠️ breadboard cap required |
| 41 | Left encoder B (PCNT) ⚠️ breadboard cap required |
| 42 | Right encoder A (PCNT) |

**Right side = front_right + rear_right motors in parallel. Left side = front_left + rear_left motors in parallel.**

⚠️ **GPIO 40/41 EMI:** Left encoder signal path requires 100 nF ceramic caps from GPIO 40 → GND and GPIO 41 → GND on the breadboard. The root EMI cause was a bad breadboard section (confirmed 2026-05-30 by GPIO swap test) — not the ESP32 pins themselves. Firmware uses PCNT (`ESP32Encoder` library, `setFilter(400)`) for hardware glitch filtering; `VEL_ALPHA = 1.0` (EMA disabled). Do not reintroduce EMA unless raw velocity is noisy on a different chassis.

**Avoid:** GPIO 4,5,6,7 (not broken out), 25,26,27,32,33 (not broken out), 35/36/37 (internal flash), 38 (RGB LED on DevKitC-1 — but Lonely Binary uses GPIO 48 for RGB LED), 43/44 (UART0 / CH340), 0/45/46 (strapping pins).
GPIO 19/20 = native USB CDC (micro-ROS + flashing — do not repurpose). GPIO 43/44 = UART0 via CH340 (display telemetry Phase 6).

---

## Encoder Constants (validated)

| Parameter | Value | Notes |
|---|---|---|
| `ENC_CPR` | 1010 | 2× quadrature, 45:1 gear ratio — validated on floor |
| `wheel_radius` | 0.03414 m | Measured (68.27 mm dia) |
| `wheel_separation` | 0.177 m | Measured center-to-center |

Encoder wire colors (JGA25-371): Red/White = motor power, Blue/Black = encoder power, Yellow = Ch A, Green = Ch B.

---

## Serial Transport (ESP32 ↔ Pi) — Dual Port

The ESP32-S3 uses **two independent serial connections** to the Pi, on two separate USB ports:

| Role | ESP32 | Pi device | Purpose |
|---|---|---|---|
| micro-ROS + flashing | Native USB CDC, GPIO 19/20 (built-in USB-JTAG, VID 303a:1001) | `/dev/ttyACM0` | ROS topics: odom, IMU, cmd_vel; auto-reset flashing |
| Display telemetry (Phase 6) | UART0, GPIO 43 TX / 44 RX via Lonely Binary CH340 (VID 1a86:7522) | `/dev/ttyUSB0` | Reserved — unused until Phase 6 |

**Why two ports:** CH340 UART0 is reserved for future use (Phase 6). During Phases 1–5, `/dev/ttyUSB0` is unused. Battery monitoring is handled by the Pi-side INA219 node, not the CH340.

**Firmware transport (validated):** `Serial` (native USB CDC) via four custom `arduino_transport_*` weak-function overrides. Key details:
- `Serial.setTxTimeoutMs(100)` — prevents partial writes during XRCE entity creation
- `Serial.begin(921600)` — 921600 baud required for odom + IMU bandwidth at 30 Hz
- Both `pub_odom` and `pub_imu` use `rclc_publisher_init_default` (RELIABLE) — `nav_msgs/Odometry` serializes to ~712 bytes which exceeds the 512-byte XRCE MTU; RELIABLE supports fragmentation, BEST_EFFORT silently drops oversized messages

**Firmware build flags:**
```ini
-DARDUINO_USB_CDC_ON_BOOT=1   ; enables native USB CDC (Serial) for micro-ROS
-DARDUINO_USB_MODE=1          ; uses built-in hardware USB-JTAG/Serial controller
```

**Flash from Pi — stop microros-agent.service first (it grabs the port mid-write):**
```bash
# 1. Stop and mask the agent so it cannot restart during flash
sudo systemctl stop microros-agent.service
sudo systemctl mask microros-agent.service
sudo fuser -k /dev/ttyACM0 2>/dev/null        # kill any stale agent process
sudo fuser /dev/ttyACM0 2>&1 || echo 'port free'   # verify clear

# 2. Flash
python3 -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
    --before default-reset --after hard-reset \
    write-flash --flash-mode dio --flash-freq 80m --flash-size detect \
    0x10000 firmware.bin

# 3. Restore
sudo systemctl unmask microros-agent.service
sudo systemctl start microros-agent.service
```

**Why:** `microros-agent.service` is independent of `robot-launch.service` and auto-restarts. It grabs `/dev/ttyACM0` between esptool's 1200bps reset touch and the write connection, causing consistent mid-write `StopIteration` failures that look like USB hardware problems but are pure port contention. Stopping only `robot-launch.service` is not enough.

**Run micro-ROS agent on Pi:**
```bash
source /opt/ros/jazzy/setup.bash
source ~/microros_ws/install/setup.bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -b 921600
```

**See also:** [`docs/testing/phase1_firmware_validation_2026-05-24.md`](docs/testing/phase1_firmware_validation_2026-05-24.md) — full log of transport issues discovered during Phase 1 bringup.

---

## ROS 2 Topic Architecture

| Topic | Publisher | Consumer | Rate |
|---|---|---|---|
| `/diff_cont/cmd_vel_unstamped` | Nav2 / twist_mux | ESP32 (via micro-ROS) | 20 Hz |
| `/diff_cont/odom` | ESP32 micro-ROS | robot_localization EKF | 30 Hz |
| `/imu/imu` | ESP32 micro-ROS | robot_localization EKF | 30 Hz |
| `/battery_state` | Pi `battery_publisher` node (INA219 via pi-ina219 library) | monitoring nodes, display | 1 Hz |
| `/odom` | robot_localization | Nav2, SLAM | 20 Hz |
| `/scan` | rplidar_node | slam_toolbox, Nav2 obstacle layer | ~5.5 Hz |
| `/camera/camera/depth/color/points` | realsense2_camera | Nav2 voxel layer | ~13 Hz |
| `/camera/camera/color/image_raw` | realsense2_camera | future: YOLO | 15 Hz |
| `/camera/camera/depth/image_rect_raw` | realsense2_camera | depth consumers | 15 Hz |

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
bot_ws/                               ← workspace root (this repo)
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
│   ├── architecture/
│   │   ├── system_overview.md
│   │   └── build_plan.md         ← step-by-step build instructions for Claude Code
│   └── testing/                      ← test protocols, validation checklists, audit reports
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
| Test protocol, validation checklist, or audit report | `docs/testing/` |
| Utility shell script | `scripts/` |

---

## docs/testing/ Rules

This folder holds all test and validation documentation. Do not put test scripts here — those go in `src/<package>/test/`. This folder is for human-readable docs only.

| Document type | Naming convention | Notes |
|---|---|---|
| Gate / MVP test protocol | `<feature>_test_protocol_<YYYY-MM-DD>.md` | Operator checklists with pass/fail gates |
| Validation run log | `<feature>_validation_<YYYY-MM-DD>.md` | Recorded results from a specific test session |
| Repo / system audit | `audit_<YYYY-MM-DD>.md` | Static analysis or manual code/hardware audit |
| Definition of Done | `dod_<feature>.md` | Acceptance criteria for a subsystem or milestone |

**Rules:**
- Every file must have a date in the filename (`YYYY-MM-DD`).
- Protocols describe *how* to test. Logs describe *what happened* during a test. Keep them separate.
- Do not commit incomplete or in-progress logs — finish the run, then commit results.
- Gate-style protocols (pass/fail per gate) are preferred over freeform notes.

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

## ROS 2 Development Standards

These rules apply to every ROS 2 package written in this project. Based on [Henki ROS 2 Best Practices](https://github.com/henki-robotics/henki_ros2_best_practices).

### Nodes
- Each node has a single responsibility. Do not combine unrelated concerns in one node.
- Separate application logic from ROS 2 communication. Put the core logic in a plain class or library; the ROS node only handles pub/sub/parameters. This makes the logic unit-testable without spinning up a ROS environment.

### Launch Files
- Use **XML** launch files (`.launch.xml`), not Python, unless Python is genuinely required for dynamic logic.
- Never pass or hardcode node parameters in launch files. Use config YAML files and load them with `<param from="..."/>`.

### Parameters
- All node parameters live in YAML files under `src/<package>/config/`.
- Never hardcode parameter values in node source code.
- If a parameter must be changeable at runtime, implement a parameter callback (`add_on_set_parameters_callback`).

### Logging
- Use the ROS 2 logger (`RCLCPP_INFO`, `RCLCPP_WARN`, `RCLCPP_ERROR` / `self.get_logger()`) — never `print()` or `std::cout`.
- Log levels: `INFO` = normal operation, `WARN` = unexpected but recoverable, `ERROR` = system no longer operating correctly.
- **Never log inside high-frequency callbacks without throttling.** The IMU, odom, and encoder callbacks run at 30–100 Hz. Use `RCLCPP_INFO_THROTTLE` / `get_logger().throttled` to cap log rate at 1 Hz or less for any message inside these loops.

### Message Interfaces
- Reuse standard interfaces first: `geometry_msgs`, `sensor_msgs`, `nav_msgs`, `std_srvs`.
- Custom message types go in `src/robot_msgs/` — already set up. Do not define custom messages inside other packages.
- Do not use primitive `std_msgs` types (`Float32`, `Bool`, `String`) in production code — create a semantically named custom message instead.

### Actions and Services
- Services only for fast operations (<1 second): state requests, mode switches, parameter gets/sets.
- Actions for anything that takes time, can fail in multiple ways, or needs cancellation (Nav2 goals, SLAM operations).
- Use enum constants for action error codes, not freeform strings, so clients can parse them reliably.

### Executors
- Use `SingleThreadedExecutor` by default. It is deterministic, easier to test, and has lower overhead.
- Only use `MultiThreadedExecutor` when a callback genuinely blocks and concurrent execution is required — not as a default.

### Performance
- Use C++ for any node with a high-frequency control loop or high-bandwidth data (encoders, motor control, point cloud processing).
- Use Python for tooling, high-level orchestration, and the display daemon.
- For the RealSense point cloud pipeline (Phase 6+): use **composable nodes** with intra-process communication to avoid copying large point cloud data over DDS.

### Package Documentation
Every package under `src/` must have a `README.md` covering:
- What the package does (one paragraph)
- How to launch it
- All published and subscribed topics, with message types and rates
- All services and actions
- All parameters with type, description, and default value

### QoS (Quality of Service)
Define QoS profiles in YAML config files — never hardcode them in node source. This lets you tune reliability and history without recompiling.

QoS profiles for this project:

| Topic | Profile | Reason |
|---|---|---|
| `/diff_cont/odom`, `/imu/imu` | `sensor_data` (BEST_EFFORT, VOLATILE) | 30 Hz — a dropped message is immediately replaced |
| `/scan` | `sensor_data` (BEST_EFFORT, VOLATILE) | 5.5 Hz sensor stream |
| `/camera/depth/points`, `/camera/color/image_raw` | `sensor_data` (BEST_EFFORT, VOLATILE) | High-bandwidth, latency-sensitive |
| `/diff_cont/cmd_vel_unstamped` | `system_default` (RELIABLE, VOLATILE) | Safety-critical — must not silently drop velocity commands |
| `/battery_state` | `system_default` (RELIABLE, VOLATILE) | Low rate, must not miss battery cutoff alerts |
| `/map`, `/odom`, `/tf` | `system_default` (RELIABLE, TRANSIENT_LOCAL) | State data — late-joining nodes must receive last known value |

In Python nodes, set QoS via `rclpy.qos.QoSProfile`. In C++ nodes, use `rclcpp::QoS`. Load the profile selection from the node's YAML config so it can be overridden without code changes.

### Node Architecture Pattern (Odom / IMU Pipeline)
The encoder-to-odometry pipeline in this project is directly analogous to the Henki speed monitor example. Apply the same before/after pattern:

**Do not do this** (logic inside the ROS callback):
```python
def odom_callback(self, msg):
    # raw encoder math, unit conversion, filtering all here
    speed_mps = msg.twist.twist.linear.x
    speed_kph = speed_mps * 3.6
    self.publisher_.publish(...)
```

**Do this instead** (logic in a separate class):
```python
# odometry_converter.py — no ROS imports, fully unit-testable
class OdometryConverter:
    def compute_speed(self, linear_x_mps):
        return linear_x_mps * 3.6, linear_x_mps * 2.237

# esp32_bridge_node.py — ROS layer only
class Esp32BridgeNode(Node):
    def __init__(self):
        self._converter = OdometryConverter()  # plain class, no ROS
    def odom_callback(self, msg):
        kph, mph = self._converter.compute_speed(msg.twist.twist.linear.x)
        self.publisher_.publish(...)
```

This pattern applies to: `esp32_serial_bridge` (serial parsing + unit conversion), `robot_localization` config (EKF math lives in the library, not the node), and any future sensor processing nodes.

### Testing
- Unit-test core application logic (the plain class, not the ROS node). Logic separated from ROS needs no ROS environment to test.
- Integration-test ROS communication behavior (topic flow, parameter loading, launch files).
- Target 90%+ coverage on core logic.
- Never use `time.sleep()` or `rclcpp::sleep_for()` in tests to wait for messages. Use synchronization primitives or a timed spin with a proper timeout.

---

## Critical Design Rules

### Safety
- ESP32 safety watchdog must run continuously, independent of ROS, Wi-Fi, and the dev PC.
- If no velocity command is received within the timeout window, ESP32 stops the motors immediately.
- Battery monitoring and low-voltage cutoff run on the Pi (`battery_publisher` node, `esp32_serial_bridge` package). When voltage drops below 9.9V the node publishes zero Twist to `/diff_cont/cmd_vel_unstamped` at 40 Hz and initiates OS shutdown after 30s. Hysteresis: exits cutoff above 10.2V.
- **Cutoff limitation (Option 1):** The 40 Hz zero publish races Nav2/teleop — it wins in practice but has no true priority. **TODO (Phase 5):** add `twist_mux` with cutoff topic at highest priority so the cutoff gate is guaranteed, not a race.
- **Dev PC failure must never cause a dangerous robot.**

### Electrical
- All subsystems share one common ground: Battery −, ESP32, Pi, TB6612FNG, encoders, sensors.
- Missing common ground causes serial errors, PWM noise, encoder EMI, and motor glitches.
- GPIO 40/41 (left encoder) require 100 nF ceramic caps to GND. Root cause of EMI was a bad breadboard section in the signal path, not the ESP32 pins. Caps are still required to guard against future noise.

### Motion Control
- Closed-loop PID velocity control only (encoder feedback → wheel velocity target in rad/s).
- Never use open-loop PWM for normal operation.
- Right side (Ch1) = front_right + rear_right in parallel. Left side (Ch2) = front_left + rear_left in parallel.
- **Nav2 Jazzy breaking change:** Nav2 Jazzy defaults to `geometry_msgs/TwistStamped` on `cmd_vel`. Set `enable_stamped_cmd_vel: false` in the Nav2 `controller_server` config to keep `geometry_msgs/Twist` on `/diff_cont/cmd_vel_unstamped`. The ESP32 firmware uses Twist — do not change this without updating the micro-ROS subscriber type.

### micro-ROS
- Transport is native USB CDC (GPIO 19/20, `Serial`) → Pi `/dev/ttyACM0` at 921600 baud. Not Serial1, not CH340, not Wi-Fi.
- CH340 UART0 (GPIO 43/44, `/dev/ttyUSB0`) is reserved for display telemetry JSON (Phase 6) — do not use it for micro-ROS.
- `pub_odom` and `pub_imu` must use `rclc_publisher_init_default` (RELIABLE), not BEST_EFFORT. `nav_msgs/Odometry` is ~712 bytes serialized — exceeds the 512-byte XRCE MTU and is silently dropped on BEST_EFFORT streams.
- Never change serial device, encoder pins, motor polarity, or controller YAML all at once during debugging. Change one thing, observe, repeat.
- If `micro_ros_agent` gets stuck after OTA flash or watchdog reset: `sudo systemctl restart microros-agent.service`.
- **Before flashing firmware:** always `sudo systemctl stop microros-agent.service && sudo systemctl mask microros-agent.service` first. The service auto-restarts independently of `robot-launch.service` and grabs `/dev/ttyACM0` mid-write. Unmask and restart after flash completes.

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

## Build Plan

The full step-by-step build plan — with files to create, implementation details, and validation gates — is in:

**[`docs/architecture/build_plan.md`](docs/architecture/build_plan.md)**

### Rules for Claude Code

- **Always run commands directly** — do not present shell commands and ask the user to run them. SSH to `ubuntu@pi5bot` for Pi commands, run `pio run` / `pio run --target upload` for firmware. The user expects Claude Code to execute, not narrate.
- **Read `build_plan.md` before starting any implementation work.** It is the authoritative source for what to build next.
- **Follow phases in order.** Do not implement Phase N+1 until Phase N's validation gate passes.
- **Update the status table** in `build_plan.md` when a phase completes.
- **Do not change hardware constants** (GPIO pins, I2C addresses, topic names, frame IDs, encoder CPR, wheel dimensions) without explicit user instruction. These are validated hardware values.
- **One thing at a time.** When debugging, change one variable (param, pin, config) and observe before changing another.
- **Commit at phase boundaries** using the commit prefix convention in `build_plan.md`.

### Motor Test Safety Rules

These rules apply every time motors are commanded to move during testing. **The robot has proven strong enough to damage itself and injure people — there is no grace period on these rules.**

#### Approved cmd_vel commands (the only two forms allowed)

```bash
# Drive — foreground, blocks until complete:
ros2 topic pub --times 160 --rate 20 /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist "{linear: {x: 0.10}}"

# Stop — foreground, single shot:
ros2 topic pub --once /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist "{linear: {x: 0.0}, angular: {z: 0.0}}"
```

#### Emergency stop (paste into any terminal)
```bash
for i in {1..20}; do ros2 topic pub --once /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist "{linear: {x: 0.0}, angular: {z: 0.0}}"; sleep 0.05; done
```

#### Hard rules

- **NEVER background a cmd_vel publisher.** `run_in_background: true` on a cmd_vel publisher is forbidden. A queued stop runs and finishes while the background publisher is still live — the robot keeps going. Robot hit a wall 2026-05-30 from this exact mistake.
- **All motor tests must use `scripts/motor_test.sh`** — the script handles bag start, foreground drive, foreground stop loop, and bag cleanup atomically. No manual background publishers.
- **Never use `timeout N cmd_vel_pub` as the only stop mechanism.** Use `--times N --rate 20` instead.
- **30-second hard ceiling on any motor-on period.** For velocity tests, 8 seconds at 0.10 m/s = 0.8 m — that is enough. Never run longer than needed.
- **Confirm stop succeeded before doing anything else.** Ask the user to confirm the robot is stationary before analysis or next step.

### Physical Action Rules

When a test requires the user to do something physical (push the robot, press a button, place it at a mark, plug a cable):

1. **State clearly what physical action is needed.**
2. **Stop — do not queue or run any follow-up command.**
3. **Wait for the user's explicit reply** ("done", "ready", "ok") before issuing any command that depends on that action having happened.

This applies to: pushing/moving the robot, pressing BOOT or RESET, plugging/unplugging cables, placing the robot on the floor or at a start mark.

### Test Reporting Rules

After every test that involves the robot moving, a sensor reading, or a subsystem check, report all four of these:

1. **Test ran:** what command was executed, duration, parameters
2. **Results:** raw numbers (velocity, position delta, rate, etc.)
3. **Expected behavior:** what should have happened
4. **Measured behavior:** what actually happened, and how it compares to expected

Format as a table or clearly labeled sections. Include specific numbers — not just "pass" or "looks good". Always compare measured to expected explicitly.

### Before Changing Things

When about to make a non-trivial firmware or config change, or when hitting an error with multiple possible causes:

1. **Pause and explain:** what the problem is, what change is being considered, what the tradeoffs are.
2. **Wait for the user to confirm direction** before proceeding.

The user guides the process — do not make architectural decisions and present finished diffs.

### Development Order (summary)

| Phase | Goal |
|---|---|
| 0 | Hardware & environment (complete) |
| 1 | ESP32 firmware: PID, encoders, IMU, micro-ROS, watchdog |
| 2 | URDF + TF tree validated in RViz2 |
| 3 | Sensor bridge + **LiDAR verified on `/scan`** + RealSense + EKF → smooth `/odom` — LiDAR must pass before Phase 4 |
| 4 | SLAM: build and save a consistent 2D map (requires Phase 3 LiDAR verified) |
| 5 | Nav2: autonomous navigation — **MVP milestone** |
| 6 | BME680 env sensor + RealSense voxel costmap |
| 7 | Semantic perception (YOLO on dev PC GPU) |

---

## Useful Debugging Commands

```bash
# Verify devices on Pi
ls /dev/rplidar
ls /dev/serial/by-id/usb-Espressif*

# micro-ROS agent (native USB CDC)
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -b 921600

# Monitor display telemetry stream (CH340 UART0 — Phase 6 only)
cat /dev/ttyUSB0

# Topic health
ros2 topic hz /diff_cont/odom
ros2 topic hz /imu/imu
ros2 topic hz /scan
ros2 topic echo /battery_state

# TF debugging
ros2 run tf2_tools view_frames
ros2 run tf2_ros tf2_echo base_link laser

# I2C sensor check
sudo i2cdetect -y 1   # Pi I2C-1: expect 0x40 (INA219)
# ESP32 I2C (BNO055 at 0x28) visible only via ESP32 debug output — not accessible from Pi directly

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
| OS (Pi) | Ubuntu Server 24.04 LTS (64-bit) — hostname: `pi5bot`, user: `ubuntu` |
| OS (dev PC) | Ubuntu 24.04 LTS — required for ROS 2 Jazzy binary packages |
| ROS | ROS 2 Jazzy Jalisco |
| Pi | Raspberry Pi 5 |
| ESP32 firmware | PlatformIO + Arduino framework — `firmware/esp32/` |
| micro-ROS agent | Built from source in `~/microros_ws` (jazzy branch) |
| Python | 3.12+ for ROS nodes |
| Dev PC GPU | NVIDIA RTX preferred for YOLO / point cloud processing |

---

## Audit Notes (2026-05-24)

Issues found during static audit of the repo. Items resolved are noted inline.

### Critical — will cause wiring mistakes or runtime failures

**~~README.md GPIO table reflects TB6612FNG pinout, not Cytron MDD10A~~** ✅ RESOLVED 2026-05-25
TB6612FNG is now the confirmed current motor driver. Firmware (motors.h/cpp) and CLAUDE.md GPIO table updated to match. Future upgrade to a larger driver + 2 additional wheels is planned.

**~~README.md GPIO 19/20 mislabeled as micro-ROS~~** ✅ RESOLVED 2026-05-24
GPIO 19/20 = native USB CDC = micro-ROS on `/dev/ttyACM0` (this is correct). GPIO 43/44 = CH340 UART0 = display telemetry on `/dev/ttyUSB0`. Rules files corrected.

### High — incorrect information that will confuse development

**README.md OS and ROS versions are stale**
`README.md` lines 34–35 say `Ubuntu 22.04 LTS` and `ROS 2 Humble Hawksbill`. The project targets **Ubuntu 24.04 LTS** and **ROS 2 Jazzy Jalisco**. Every other doc in the repo is correct.

**`docs/hardware/development_pc.md` has two errors in the software stack table**
- Line 26: `Ubuntu | 22.04 LTS` — should be **24.04 LTS** (contradicts line 18 of the same file).
- Line 27: `ROS 2 | Jazzy Hawksbill` — the release name is **Jazzy Jalisco** (Hawksbill comes from Humble Hawksbill; the names were mixed).

**`docs/architecture/autonomous_robot_system_specification_v1.md` line 513 — wrong Pi OS**
States `Raspberry Pi OS Trixie (Pi)`. The Pi runs **Ubuntu Server 24.04 LTS**, not Raspberry Pi OS.

### Warning — code quality / maintainability

**~~`battery_node.py` INA219 configured with GAIN_AUTO default causing 32.76V / NaN~~** ✅ FIX COMMITTED 2026-05-30 — verify next session
`configure()` without `max_expected_amps` initialised with `GAIN_1_40MV` (max 0.4A at 0.1Ω). Logic rail draws well over 0.4A, triggering OVF. Bus voltage register reads 0x7FFF = 32.764V; current() garbage. Fix: `INA219(SHUNT_OHMS, max_expected_amps=3.0)` + `configure(voltage_range=RANGE_32V, gain=GAIN_8_320MV)` + voltage sanity check (1–20V). **Do not trust low-voltage cutoff until confirmed reading correctly (expect ~9.9–12.6V).**

**`firmware/esp32/src/motors.cpp` uses deprecated arduino-esp32 2.x LEDC API**
`ledcSetup()` and `ledcAttachPin()` are the 2.x API. `platformio.ini` pins `espressif32@^6.8.0` which bundles arduino-esp32 **3.2.x**, where these are deprecated. The code compiles via backwards-compat shims but generates warnings and is one platform upgrade away from breaking. The 3.x replacements are `ledcAttach(pin, freq, resolution)` and `ledcWrite(pin, duty)`.

**`firmware/esp32/include/encoders.h` line 11 — inaccurate CPR derivation comment**
Comment says `"2x quadrature decoding (PCNT half-quad mode) of 11 PPR motor encoder through 45:1 gearbox"`. 11 × 2 × 45 = 990, not 1010. The constant `ENC_CPR = 1010` is correct (floor-validated); the 11 PPR figure in the comment is slightly off.

### Info — missing required documentation

**Five of six ROS packages missing `README.md`** (required by project standards)
Missing from: `esp32_serial_bridge`, `robot_bringup`, `robot_navigation`, `robot_slam`, `robot_msgs`. Only `robot_description` has one.
