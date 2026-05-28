# ESP32-S3 Firmware

## Framework

PlatformIO + Arduino (espressif32 ^6.8.0 / arduino-esp32 ~3.2.x)

## Structure

```
firmware/esp32/
├── platformio.ini        # Build config, lib deps, build flags
├── src/
│   ├── main.cpp          # setup() + loop(): PID at 100 Hz, odom+IMU at 30 Hz, battery at 1 Hz
│   ├── motors.cpp        # TB6612FNG LEDC PWM + direction (1 kHz, 8-bit)
│   ├── encoders.cpp      # ESP32Encoder PCNT quadrature (hardware, no ISR)
│   ├── pid.cpp           # PID velocity controller with anti-windup
│   ├── imu.cpp           # BNO055 IMUPLUS mode (accel + gyro, no magnetometer)
│   ├── battery.cpp       # INA219 reader + FreeRTOS task (Core 0)
│   └── microros.cpp      # micro-ROS state machine, publishers, cmd_vel subscriber
└── include/
    ├── motors.h          # MOTOR_MAX_RAD_S=6.5, MOTOR_MIN_DUTY=0.25, PWM_FREQ=1000
    ├── encoders.h        # ENC_CPR=1010 (validated on floor)
    ├── pid.h             # KP=1.4, KI=0.38, KD=0.0, INTEGRAL_LIMIT=40.0
    ├── imu.h
    ├── battery.h
    └── microros.h
```

## Key Responsibilities

- PID closed-loop velocity control at 100 Hz (Core 1 / `loop()`)
- Hardware quadrature encoder counting via ESP32 PCNT peripheral (ESP32Encoder)
- BNO055 IMU: linear acceleration + angular velocity at 30 Hz
- INA219 battery monitor: voltage, current, power — FreeRTOS task on Core 0, published at 1 Hz via micro-ROS
- Safety watchdog: motors stop within 500 ms of last cmd_vel
- Battery voltage cutoff: `motors_stop()` below 9.9 V (independent of ROS)
- micro-ROS over native USB CDC (GPIO 19/20 → Pi `/dev/ttyACM0` at 921600 baud)

## Serial Transport

| Port | ESP32 | Pi device | Baud | Purpose |
|---|---|---|---|---|
| Native USB CDC | GPIO 19/20 (built-in USB-JTAG) | `/dev/ttyACM0` | 921600 | micro-ROS topics + flashing |
| CH340 UART0 | GPIO 43 TX / 44 RX | `/dev/ttyUSB0` | — | Reserved — display telemetry Phase 6+ |

The micro-ROS transport uses four `arduino_transport_*` weak-function overrides to drive `Serial` (native USB CDC). `pub_odom` and `pub_imu` use `rclc_publisher_init_default` (RELIABLE) because `nav_msgs/Odometry` serializes to ~712 bytes, which exceeds the 512-byte XRCE MTU and is silently dropped on BEST_EFFORT streams.

## ROS Topics

| Topic | Type | Direction | Rate |
|---|---|---|---|
| `/diff_cont/odom` | `nav_msgs/Odometry` | publish | 30 Hz |
| `/imu/imu` | `sensor_msgs/Imu` | publish | 30 Hz |
| `/battery_state` | `sensor_msgs/BatteryState` | publish | 1 Hz |
| `/diff_cont/cmd_vel_unstamped` | `geometry_msgs/Twist` | subscribe | 20 Hz |

## PID Constants (validated — do not change without floor tuning)

| Constant | Value | Notes |
|---|---|---|
| `MOTOR_MAX_RAD_S` | 6.5 | Measured no-load max on test chassis (0.218 m/s) |
| `MOTOR_MIN_DUTY` | 0.25 | Deadband floor — below this gearbox doesn't move |
| `MOTOR_PWM_FREQ` | 1000 Hz | 20 kHz caused 10× speed loss due to motor inductance |
| `PID_KP` | 1.4 | Scaled from articubot_one: 55 × 6.5/255 |
| `PID_KI` | 0.38 | Scaled from articubot_one: 15 × 6.5/255; needs floor tuning |
| `PID_KD` | 0.0 | Not needed — derivative amplifies encoder noise |
| `INTEGRAL_LIMIT` | 40.0 | Anti-windup clamp (rad/s units) |

## Build Flags (platformio.ini)

```ini
-DARDUINO_USB_CDC_ON_BOOT=1   ; enables Serial (native USB CDC) for micro-ROS
-DARDUINO_USB_MODE=1          ; uses built-in hardware USB-JTAG/Serial controller
-DCORE_DEBUG_LEVEL=0          ; suppresses Arduino debug output on UART0
```

## Running the micro-ROS Agent (on Pi)

The agent runs as a systemd service (`microros-agent.service`) and starts at boot. To run manually:

```bash
source /opt/ros/jazzy/setup.bash
source ~/microros_ws/install/local_setup.bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -b 921600
```

## Flashing from Pi (no button press needed)

```bash
python3 -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
    --before default-reset --after hard-reset \
    write-flash --flash-mode dio --flash-freq 80m --flash-size detect \
    0x10000 firmware.bin
```

Stop the micro-ROS agent before flashing (it holds `/dev/ttyACM0`):
```bash
sudo systemctl stop microros-agent.service
# flash here
sudo systemctl start microros-agent.service
```

## Version Constraints

- **espressif32 ^6.8.0** — bundles arduino-esp32 ~3.2.x
  - Minimum 6.3.0 for BNO055 I2C clock-stretching fix (ESP-IDF ≥ 5.4.0)
  - Do NOT use arduino-esp32 ≥ 3.3.6: UART regression breaks custom UART pin assignment
- LEDC API: arduino-esp32 3.x uses `ledcAttach(pin, freq, bits)` / `ledcWrite(pin, duty)` (not the deprecated 2.x `ledcSetup`/`ledcAttachPin`)
