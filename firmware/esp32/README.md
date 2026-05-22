# ESP32-S3 Firmware

## Framework

PlatformIO + Arduino (espressif32 ^6.8.0 / arduino-esp32 ~3.2.x)

## Structure

```
firmware/esp32/
├── platformio.ini        # Build config, lib deps, build flags
├── src/
│   ├── main.cpp          # setup() + loop(): PID at 100 Hz, odom+IMU at 30 Hz, battery at 1 Hz
│   ├── motors.cpp        # TB6612FNG LEDC PWM + direction (20 kHz, 8-bit)
│   ├── encoders.cpp      # ESP32Encoder PCNT quadrature (hardware, no ISR)
│   ├── pid.cpp           # PID velocity controller with anti-windup
│   ├── imu.cpp           # BNO055 IMUPLUS mode (accel + gyro, no magnetometer)
│   ├── battery.cpp       # INA219 reader + FreeRTOS task (Core 0)
│   └── microros.cpp      # micro-ROS state machine, publishers, cmd_vel subscriber
└── include/
    ├── motors.h
    ├── encoders.h
    ├── pid.h
    ├── imu.h
    ├── battery.h
    └── microros.h
```

## Key Responsibilities

- PID closed-loop velocity control at 100 Hz (Core 1 / `loop()`)
- Hardware quadrature encoder counting via ESP32 PCNT peripheral (ESP32Encoder)
- BNO055 IMU: linear acceleration + angular velocity at 30 Hz
- INA219 battery monitor: voltage, current, power at 5 Hz on Core 0 (FreeRTOS task)
- Safety watchdog: motors stop within 500 ms of last cmd_vel
- Battery voltage cutoff: `motors_stop()` below 9.9 V (independent of ROS)
- micro-ROS over Serial1 UART (GPIO 17 TX / 18 RX → Pi `/dev/ttyUSB0`)
- Display telemetry JSON over Serial0 USB CDC (GPIO 19/20 → Pi `/dev/ttyACM0`)

## Serial Ports

| Port | GPIO | Pi device | Purpose |
|---|---|---|---|
| Serial1 UART | 17 TX / 18 RX | `/dev/ttyUSB0` | micro-ROS topics |
| Serial0 USB CDC | 19/20 | `/dev/ttyACM0` | Battery telemetry JSON at 1 Hz |

## ROS Topics

| Topic | Type | Direction | Rate |
|---|---|---|---|
| `/diff_cont/odom` | `nav_msgs/Odometry` | publish | 30 Hz |
| `/imu/imu` | `sensor_msgs/Imu` | publish | 30 Hz |
| `/battery_state` | `sensor_msgs/BatteryState` | publish | 1 Hz |
| `/diff_cont/cmd_vel_unstamped` | `geometry_msgs/Twist` | subscribe | 20 Hz |

## Build Flags (platformio.ini)

```ini
-DARDUINO_USB_CDC_ON_BOOT=1           ; enables Serial0 USB CDC
-DARDUINO_USB_MODE=1
-DMICRO_ROS_TRANSPORT_ARDUINO_SERIAL  ; micro-ROS uses Serial1 UART
```

## Running the micro-ROS Agent (on Pi)

```bash
source /opt/ros/humble/setup.bash
source ~/microros_ws/install/local_setup.bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyUSB0 -b 115200
```

## Version Constraints

- **espressif32 ^6.8.0** — bundles arduino-esp32 ~3.2.x
  - Minimum 6.3.0 for BNO055 I2C clock-stretching fix (ESP-IDF ≥ 5.4.0)
  - Do NOT use arduino-esp32 ≥ 3.3.6: UART regression breaks `Serial1.begin()` on custom GPIO pins
- LEDC API: arduino-esp32 3.x uses pin-based `ledcAttach(pin, freq, bits)` / `ledcWrite(pin, duty)`
