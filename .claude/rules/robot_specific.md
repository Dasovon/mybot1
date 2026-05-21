# Robot-Specific Rules — mybot1

## Build plan is authoritative

Always read `docs/architecture/build_plan.md` before starting implementation. Follow phases in order. Do not implement Phase N+1 until Phase N validation gate passes.

## URDF / xacro rules

- All sensor frames defined relative to `base_link`
- TF must always match physical sensor placement
- Never rename joint, link, plugin, or topic identifiers without updating all dependents
- Frame IDs must match the authoritative list in `hardware_constants.md`

URDF xacro pattern:
```xml
<robot name="mybot1" xmlns:xacro="http://www.ros.org/wiki/xacro">
  <xacro:include filename="$(find robot_description)/urdf/materials.xacro"/>

  <link name="base_link"/>

  <joint name="base_to_laser" type="fixed">
    <parent link="base_link"/>
    <child link="laser"/>
    <origin xyz="0.0 0.0 0.15" rpy="0 0 0"/>
  </joint>

  <link name="laser">
    <visual>
      <geometry><cylinder radius="0.05" length="0.07"/></geometry>
    </visual>
  </link>
</robot>
```

## EKF configuration notes

- IMU orientation is **disabled** (magnetometer unreliable on metal chassis)
- IMU angular velocity and linear acceleration are **enabled**
- Input: `/imu/imu` (BNO055 via micro-ROS) + `/diff_cont/odom` (encoder odometry)
- Output: `/odom` at 20 Hz

## Nav2 configuration

- Nav2 stack runs on the **development PC**, not on the Pi
- Pi runs: micro-ROS agent, LiDAR driver, RealSense driver, robot_localization EKF, robot_state_publisher
- Dev PC runs: slam_toolbox, Nav2, RViz2, YOLO

Costmap layers (in order):
1. Static map (slam_toolbox output)
2. Obstacle layer (LiDAR `/scan`)
3. Voxel layer (RealSense `/camera/depth/points`)
4. Semantic layer (YOLO, Phase 7)

## Motor control rules

- Closed-loop PID velocity control only — encoder feedback → wheel velocity target in rad/s
- Never use open-loop PWM for normal operation
- Motor A = RIGHT wheel, Motor B = LEFT wheel — do not swap
- PWM frequency: **20 kHz** (LEDC) — inaudible motor whine, negligible heat increase on TB6612FNG
- PID loop rate: 100 Hz

PID output mapping:
```cpp
// target_rad_s → PWM duty (0–255)
float duty = pid_output / MAX_RAD_S * 255.0f;
duty = constrain(duty, 0, 255);
```

## Safety watchdog (ESP32)

- Watchdog must run on ESP32 independently of ROS, Wi-Fi, and dev PC
- If no `/diff_cont/cmd_vel_unstamped` received within timeout: stop motors immediately
- Battery voltage cutoff runs on ESP32 — never depend on ROS for battery safety
- Dev PC failure must never cause a dangerous robot

Watchdog pattern in firmware:
```cpp
unsigned long last_cmd_ms = 0;
const unsigned long WATCHDOG_TIMEOUT_MS = 500;

void check_watchdog() {
    if (millis() - last_cmd_ms > WATCHDOG_TIMEOUT_MS) {
        stop_motors();
    }
}
```

## GPIO 40/41 EMI mitigation

Left encoder (GPIO 40/41) picks up TB6612 20 kHz PWM switching noise. Two mitigations required:
1. **Hardware**: 100 nF ceramic caps from GPIO 40 → GND and GPIO 41 → GND in the encoder signal path (breadboard caps before ESP32 pins)
2. **Firmware (fallback)**: EMA velocity filter — `VEL_ALPHA = 0.2`

**Preferred firmware approach**: Use the ESP32-S3 PCNT hardware pulse counter (ESP32Encoder library — `madhephaestus/ESP32Encoder`) instead of `attachInterrupt` + EMA. PCNT has a built-in glitch filter, runs in hardware, and produces cleaner quadrature counts with no CPU overhead. Hardware caps are still required regardless of encoder approach.

## BNO055 on ESP32-S3 compatibility

Requires **arduino-esp32 ≥ 3.2.0** (espressif32 ≥ 6.3.0, ESP-IDF ≥ 5.4.0). Earlier versions have an I2C clock-stretching bug that makes BNO055 unreliable. Do NOT use arduino-esp32 3.3.6+ — it has a UART regression that breaks `Serial1.begin()` with custom GPIO pins (affects micro-ROS on GPIO 17/18). Pin in platformio.ini:
```ini
platform = espressif32@^6.8.0
```

## Display daemon

The OLED display daemon is a **ROS2-independent systemd service** on the Pi, not a ROS node. This is intentional: the display works at boot before ROS2 starts, during ROS2 crashes, and during ESP32 reflashing.

- Source: `scripts/display_daemon.py`
- Service: `scripts/mybot-display.service`
- Reads Serial0 JSON from `/dev/ttyACM0` at 2 Hz
- Renders via luma.oled over SPI0 to Waveshare 2.42" OLED (SSD1309)

## Common debugging commands

```bash
# Verify devices on Pi
ls /dev/rplidar
ls /dev/serial/by-id/usb-Espressif*

# micro-ROS agent (Serial1 UART via ttyUSB0)
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyUSB0 -b 115200

# Topic health
ros2 topic hz /diff_cont/odom
ros2 topic hz /imu/imu
ros2 topic hz /scan
ros2 topic echo /battery_state

# Display daemon monitor
cat /dev/ttyACM0

# TF debugging
ros2 run tf2_tools view_frames
ros2 run tf2_ros tf2_echo base_link laser

# I2C sensor check (on ESP32 I2C bus via Pi serial)
# expect: 0x28 (BNO055), 0x40 (INA219)

# LiDAR stale process fix
sudo fuser -k /dev/rplidar

# Restart robot launch service
sudo systemctl restart robot-launch.service
```
