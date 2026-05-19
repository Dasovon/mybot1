# Robot Build Plan — Claude Code Instructions

This document is the authoritative step-by-step build plan for this robot. Claude Code must read this before starting any implementation work and follow it in order.

**Rules for Claude Code:**
- Complete each phase fully before moving to the next. Do not skip ahead.
- Run the validation gate at the end of each phase before marking it done.
- Do not modify hardware constants, GPIO pins, topic names, or frame IDs unless CLAUDE.md explicitly authorizes a change.
- If a step produces unexpected results, stop and report — do not paper over it.
- All new files must follow the folder rules in CLAUDE.md.
- After completing each phase, commit with a message referencing the phase (e.g. `Phase 1 complete: ESP32 firmware`).

---

## Current Status

| Phase | Status |
|---|---|
| 0 — Hardware & Environment | Complete |
| 1 — ESP32 Firmware | Not started |
| 2 — ROS 2 Foundation (URDF + TF) | Not started |
| 3 — Sensor Bridge & EKF | Not started |
| 4 — SLAM | Not started |
| 5 — Nav2 Autonomous Navigation | Not started |
| 6 — Extended Sensors | Not started |
| 7 — Semantic Perception | Not started |

Update the table above as each phase completes.

---

## Phase 0 — Hardware & Environment (Complete)

Hardware is assembled and confirmed per CLAUDE.md. Dev environment is ready. No code action required.

Confirmed:
- All hardware wired per GPIO map in CLAUDE.md
- GPIO 40/41 EMI caps installed on breadboard
- Common ground verified
- Pi reachable over SSH, `/dev/ttyACM0` present, `/dev/rplidar` present

---

## Phase 1 — ESP32 Firmware

### Goal
Working closed-loop motor control with encoder feedback, IMU + battery publishing over micro-ROS, and a safety watchdog — all running at the required rates.

### Files to create

| File | Purpose |
|---|---|
| `firmware/esp32/src/main.cpp` | Entry point: setup + loop |
| `firmware/esp32/src/motors.cpp` | TB6612 PWM + direction control |
| `firmware/esp32/include/motors.h` | Motor driver interface |
| `firmware/esp32/src/encoders.cpp` | ISR-based quadrature encoder counting |
| `firmware/esp32/include/encoders.h` | Encoder interface |
| `firmware/esp32/src/pid.cpp` | PID velocity controller |
| `firmware/esp32/include/pid.h` | PID interface |
| `firmware/esp32/src/imu.cpp` | BNO055 I2C read + publish |
| `firmware/esp32/include/imu.h` | IMU interface |
| `firmware/esp32/src/battery.cpp` | INA219 I2C read + publish |
| `firmware/esp32/include/battery.h` | Battery interface |
| `firmware/esp32/src/microros.cpp` | micro-ROS node, publishers, subscriber setup |
| `firmware/esp32/include/microros.h` | micro-ROS interface |
| `firmware/esp32/platformio.ini` | PlatformIO build config |

### Step-by-step

**Step 1.1 — PlatformIO project**
Create `firmware/esp32/platformio.ini` targeting `esp32-s3-devkitc-1`. Required build flags:
```
-DARDUINO_USB_CDC_ON_BOOT=1
-DMICRO_ROS_TRANSPORT_ARDUINO_SERIAL
```
Dependencies: `micro_ros_arduino`, `Adafruit BNO055`, `Adafruit INA219`, `Wire`.

**Step 1.2 — Motor driver**
Implement TB6612 control using ESP32 LEDC peripheral. GPIO map:
- PWMA (GPIO 10) → right motor, LEDC ch 0, 1 kHz, 8-bit
- AIN1/AIN2 (GPIO 11/12) → right direction
- PWMB (GPIO 13) → left motor, LEDC ch 1, 1 kHz, 8-bit
- BIN1/BIN2 (GPIO 14/15) → left direction

Expose: `motors_set_velocity(float right_mps, float left_mps)` and `motors_stop()`.

**Step 1.3 — Encoder ISR**
Attach interrupts on GPIO 42 (right A) and GPIO 40 (left A) as CHANGE. Read B channels (GPIO 39, 41) inside ISR for direction. Constants from CLAUDE.md: `ENC_CPR = 1010`, `wheel_radius = 0.034 m`.

Apply EMA filter on left encoder velocity (`VEL_ALPHA = 0.2`) to suppress GPIO 40/41 PWM noise.

**Step 1.4 — PID controller**
One PID instance per wheel. Input: measured wheel velocity (rad/s). Output: PWM command. Run at 100 Hz in a FreeRTOS task or `loop()`. Expose tunable Kp, Ki, Kd constants via `#define` in a header.

**Step 1.5 — IMU (BNO055)**
Initialize on I2C (GPIO 8 SDA, GPIO 9 SCL, addr 0x28). Read linear acceleration and angular velocity at 30 Hz. Do not use magnetometer (unreliable on metal chassis).

**Step 1.6 — Battery monitor (INA219)**
Initialize on same I2C bus (addr 0x40). Read bus voltage and current at 1 Hz.

**Step 1.7 — Safety watchdog**
If no `/diff_cont/cmd_vel_unstamped` message is received within 500 ms, call `motors_stop()`. Watchdog must run independently of micro-ROS connection state — use a hardware timer or FreeRTOS timer, not a ROS callback.

**Step 1.8 — micro-ROS node**
Transport: USB serial (`Serial`, HWCDC). Publishers:
- `/diff_cont/odom` — `nav_msgs/Odometry` at 30 Hz
- `/imu/imu` — `sensor_msgs/Imu` at 30 Hz
- `/battery_state` — `sensor_msgs/BatteryState` at 1 Hz

Subscriber:
- `/diff_cont/cmd_vel_unstamped` — `geometry_msgs/Twist` — feed velocity targets to PID, reset watchdog

Frame IDs: `odom` → `base_link` for odometry, `imu_link` for IMU.

### Validation gate — Phase 1

Run on Pi with micro-ROS agent active:
```bash
ros2 topic hz /diff_cont/odom      # must be ~30 Hz
ros2 topic hz /imu/imu             # must be ~30 Hz
ros2 topic echo /battery_state --once   # must show plausible voltage/current
ros2 topic pub /diff_cont/cmd_vel_unstamped geometry_msgs/Twist \
  "{linear: {x: 0.1}, angular: {z: 0.0}}" --once
# robot should move forward briefly, then stop after watchdog timeout
```

Phase 1 is complete when all four checks pass.

---

## Phase 2 — ROS 2 Foundation (URDF + TF)

### Goal
A complete URDF describing the robot's physical geometry and sensor placement. TF tree verified in RViz2 with correct frame names.

### Files to create

| File | Purpose |
|---|---|
| `src/robot_description/urdf/robot.urdf.xacro` | Main robot description |
| `src/robot_description/urdf/base.xacro` | Chassis, wheel joints, diff drive plugin |
| `src/robot_description/urdf/sensors.xacro` | LiDAR, IMU, camera frames |
| `src/robot_description/launch/description.launch.py` | Launches robot_state_publisher |
| `src/robot_description/config/joint_state.yaml` | Joint state publisher config |

### Step-by-step

**Step 2.1 — Chassis and wheels**
Define `base_link` as the robot body. Add `left_wheel` and `right_wheel` as continuous joints. Use measured values from CLAUDE.md: `wheel_radius = 0.034 m`, `wheel_separation = 0.179 m`.

**Step 2.2 — Sensor frames**
Define static frames relative to `base_link`:
- `laser` — RPLidar A1 mount position
- `imu_link` — BNO055 position (on ESP32 board)
- `camera_link` — RealSense D435 mount position
- `camera_depth_frame` — child of `camera_link`

Frame positions must match physical sensor placement on the robot. Measure and record them.

**Step 2.3 — Diff drive plugin**
Add `libgazebo_ros_diff_drive` plugin (or `ros2_control` diff drive controller) configured for:
- Left joint: `left_wheel`
- Right joint: `right_wheel`
- Wheel separation: 0.179 m
- Wheel radius: 0.034 m
- Command topic: `/diff_cont/cmd_vel_unstamped`
- Odometry topic: `/diff_cont/odom`

**Step 2.4 — Launch file**
`description.launch.py` must launch `robot_state_publisher` with the xacro output and `joint_state_publisher`.

### Validation gate — Phase 2

```bash
ros2 launch robot_description description.launch.py
ros2 run tf2_tools view_frames
# Output PDF must show: map → odom → base_link → laser, imu_link, camera_link, left_wheel, right_wheel
ros2 run tf2_ros tf2_echo base_link laser
# Must return a valid static transform, no errors
```

Phase 2 is complete when `view_frames` shows the full correct TF tree.

---

## Phase 3 — Sensor Bridge & EKF

### Goal
All three sensor streams (LiDAR, RealSense, IMU/odom from ESP32) active and fused into a smooth `/odom` output by `robot_localization`.

### Files to create

| File | Purpose |
|---|---|
| `src/esp32_serial_bridge/esp32_serial_bridge/serial_bridge_node.py` | micro-ROS agent wrapper node |
| `src/esp32_serial_bridge/launch/bridge.launch.py` | Launches micro-ROS agent + bridge node |
| `src/robot_bringup/launch/sensors.launch.py` | Launches LiDAR + RealSense nodes |
| `src/robot_bringup/config/ekf.yaml` | robot_localization EKF config |
| `src/robot_bringup/launch/ekf.launch.py` | Launches robot_localization node |

### Step-by-step

**Step 3.1 — micro-ROS agent bridge**
`bridge.launch.py` must start `micro_ros_agent serial --dev /dev/ttyACM0`. Use the stable by-id path from CLAUDE.md as a fallback. The node should wait for the device to appear before launching.

**Step 3.2 — LiDAR driver**
Launch `rplidar_ros` with device `/dev/rplidar`. Frame ID must be `laser`. Expected rate: ~5.5 Hz on `/scan`.

**Step 3.3 — RealSense driver**
Launch `realsense2_camera` with:
- Resolution: 640×480
- Depth FPS: 15
- Color FPS: 15
- Backend: RSUSB
- Frame prefix: `camera`

**Step 3.4 — EKF configuration**
`ekf.yaml` fuses:
- `/diff_cont/odom` — x, y, yaw, vx, vyaw
- `/imu/imu` — angular velocity z, linear acceleration x/y

IMU orientation **disabled** (unreliable magnetometer). Set `two_d_mode: true`. Output frame: `odom`. Base frame: `base_link`.

### Validation gate — Phase 3

```bash
ros2 topic hz /scan              # ~5.5 Hz
ros2 topic hz /camera/depth/points   # ~15 Hz
ros2 topic hz /diff_cont/odom    # ~30 Hz
ros2 topic hz /odom              # ~20 Hz — EKF output
ros2 run tf2_ros tf2_echo odom base_link   # must update smoothly while robot moves
```

Drive the robot slowly in a straight line. `/odom` pose should track accurately with no jumps.

Phase 3 is complete when all topic rates are met and `/odom` tracks motion smoothly.

---

## Phase 4 — SLAM

### Goal
Build and save a consistent 2D map of a real environment using `slam_toolbox` with LiDAR as the primary sensor.

### Files to create

| File | Purpose |
|---|---|
| `src/robot_slam/config/slam_toolbox.yaml` | slam_toolbox mapper params |
| `src/robot_slam/launch/slam.launch.py` | Launches slam_toolbox in online async mode |
| `src/robot_slam/launch/localization.launch.py` | Launches slam_toolbox in localization mode |

### Step-by-step

**Step 4.1 — slam_toolbox config**
Key params in `slam_toolbox.yaml`:
- `odom_frame: odom`
- `map_frame: map`
- `base_frame: base_link`
- `scan_topic: /scan`
- `mode: mapping`
- Tune `resolution`, `max_laser_range`, and loop closure params for indoor use.

**Step 4.2 — Mapping launch**
`slam.launch.py` launches `async_slam_toolbox_node` with the config. It should also bring up `description.launch.py` and `ekf.launch.py` as dependencies (or use `robot_bringup`).

**Step 4.3 — Map save / load**
Use `slam_toolbox` save map service to persist maps to `src/robot_navigation/maps/`. Localization launch file loads a saved map and runs in localization-only mode.

### Validation gate — Phase 4

Drive one full loop of the test area, then a second overlapping loop.
```bash
ros2 service call /slam_toolbox/save_map slam_toolbox/srv/SaveMap \
  "{name: {data: 'src/robot_navigation/maps/test_map'}}"
```
- Map must show clean walls with no significant double-lines
- Loop closure must align the two traversals
- Saved map must reload without errors in localization mode

Phase 4 is complete when a saved map passes the above criteria.

---

## Phase 5 — Nav2 Autonomous Navigation

### Goal
The robot navigates autonomously to a goal pose on a known map, avoiding static and dynamic obstacles, and recovers safely when blocked.

### Files to create

| File | Purpose |
|---|---|
| `src/robot_navigation/config/nav2_params.yaml` | Full Nav2 parameter set |
| `src/robot_navigation/launch/navigation.launch.py` | Launches Nav2 stack |
| `src/robot_bringup/launch/bringup.launch.py` | Top-level launch: all subsystems |

### Step-by-step

**Step 5.1 — Nav2 params**
Configure `nav2_params.yaml` with:
- Controller: `DWBLocalPlanner` or `RPP` (regulated pure pursuit)
- Costmap layers: `static_layer` (SLAM map) → `obstacle_layer` (LiDAR `/scan`) → `voxel_layer` (RealSense, placeholder for Phase 6)
- Global/local costmap inflation radius tuned for robot footprint
- Controller frequency: 20 Hz
- Command topic: `/diff_cont/cmd_vel_unstamped`

**Step 5.2 — Navigation launch**
`navigation.launch.py` launches:
- `nav2_bringup` with `nav2_params.yaml`
- Map server with saved map from Phase 4
- AMCL for localization (or slam_toolbox localization mode)

**Step 5.3 — Top-level bringup**
`bringup.launch.py` includes:
1. `description.launch.py`
2. `bridge.launch.py` (micro-ROS)
3. `sensors.launch.py` (LiDAR + RealSense)
4. `ekf.launch.py`
5. `navigation.launch.py`

This is the single launch file used to bring the full robot up.

### Validation gate — Phase 5

```bash
ros2 launch robot_bringup bringup.launch.py
# In RViz2: send a 2D Nav Goal
```
- Robot must plan and execute a path to the goal
- Robot must stop and replan when an obstacle is introduced into the path
- Robot must stop safely if blocked and unable to recover (no unsafe motion)
- Watchdog must still trigger if Nav2 stops publishing commands

Phase 5 is complete when all four criteria pass. This is the **MVP milestone**.

---

## Phase 6 — Extended Sensors

### Goal
BME680 environmental data streaming, and RealSense depth integrated into Nav2 as a voxel costmap layer.

### Files to create / modify

| File | Purpose |
|---|---|
| `firmware/esp32/src/env_sensor.cpp` | BME680 I2C read + publish |
| `firmware/esp32/include/env_sensor.h` | BME680 interface |
| `src/robot_msgs/msg/EnvData.msg` | Custom message for BME680 data |
| `src/robot_bringup/config/nav2_params.yaml` | Enable voxel_layer with RealSense |

### Step-by-step

**Step 6.1 — BME680 firmware**
Wire BME680 to I2C bus (addr 0x76). Read temperature, humidity, pressure, gas resistance. Publish as custom `robot_msgs/EnvData` or `sensor_msgs/Temperature` + `sensor_msgs/RelativeHumidity` at 1 Hz. Add to micro-ROS node.

**Step 6.2 — RealSense voxel layer**
In `nav2_params.yaml`, enable `voxel_layer` in both global and local costmaps. Subscribe to `/camera/depth/points`. Tune height range to detect obstacles between 0.05 m and 1.5 m above floor.

### Validation gate — Phase 6

```bash
ros2 topic echo /env_data --once    # must show plausible temp/humidity/pressure
ros2 topic hz /camera/depth/points  # still ~15 Hz
# In RViz2: 3D obstacle visible in local costmap when object held in front of camera
```

---

## Phase 7 — Semantic Perception

### Goal
YOLO running on dev PC GPU classifies objects detected in the RealSense color stream. Classifications feed into a semantic costmap layer.

### Files to create

| File | Purpose |
|---|---|
| `src/robot_bringup/launch/yolo.launch.py` | Launches YOLO inference node on dev PC |
| `src/robot_navigation/config/semantic_costmap.yaml` | Semantic layer config |

### Step-by-step

**Step 7.1 — YOLO node**
Subscribe to `/camera/color/image_raw`. Run YOLOv8 inference. Publish detections as `vision_msgs/Detection2DArray` on `/detections`. Target: ≥10 FPS on dev PC GPU.

**Step 7.2 — Semantic costmap layer**
Use `nav2_costmap_2d` custom layer or a plugin to inflate costs around detected obstacles of specific classes (e.g. `person` → high cost zone).

### Validation gate — Phase 7

```bash
ros2 topic hz /detections    # ≥10 Hz
# In RViz2: semantic costmap layer inflates around a person standing in camera view
```

---

## Commit Conventions Per Phase

| Phase complete | Commit message prefix |
|---|---|
| Phase 1 | `Phase 1: ESP32 firmware — <description>` |
| Phase 2 | `Phase 2: URDF/TF — <description>` |
| Phase 3 | `Phase 3: Sensors/EKF — <description>` |
| Phase 4 | `Phase 4: SLAM — <description>` |
| Phase 5 | `Phase 5: Nav2 — <description>` |
| Phase 6 | `Phase 6: Extended sensors — <description>` |
| Phase 7 | `Phase 7: Semantic — <description>` |
