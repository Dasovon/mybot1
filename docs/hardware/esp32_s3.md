# ESP32-S3 — Motion Controller

## Role in This Project

The ESP32-S3 is the embedded base controller. It runs independently of ROS and the development PC and is responsible for all real-time and safety-critical motor operations.

---

## Responsibilities

| Domain | Details |
|---|---|
| Motor control | PWM output via TB6612FNG (1 kHz, 8-bit) |
| Encoder reading | PCNT hardware counter with `setFilter(400)` glitch rejection |
| PID velocity control | Closed-loop wheel velocity (rad/s), 100 Hz loop |
| IMU reading | BNO055 via I2C (addr 0x28) |
| Environmental sensing | BME680 via I2C (addr 0x76) — planned, not yet wired |
| Safety watchdog | Motor stop on cmd_vel timeout |
| micro-ROS publisher | `/diff_cont/odom`, `/imu/imu` at 30 Hz; subscriber for `/diff_cont/cmd_vel_unstamped` |
| Debug console | ASCII state/timing output on UART0 via CH340 → Pi `/dev/ttyUSB0` at 115200 baud |

Battery monitoring is **not** an ESP32 responsibility. It is handled by the Pi-side `battery_publisher` node reading the INA219 on Pi I2C-1.

---

## Key Specs

| Property | Value |
|---|---|
| MCU | Xtensa LX7 dual-core, up to 240 MHz |
| Flash | 8 MB |
| Wi-Fi | 802.11 b/g/n (not used) |
| Bluetooth | BLE 5.0 (not used) |
| I2C | Hardware I2C bus; hosts BNO055 and BME680 (planned) |
| GPIO | 45 usable pins on DevKitC-1 |

---

## GPIO Function Map

All GPIO is 3.3V logic. 3V3 pin powers TB6612FNG logic, BNO055, and BME680 directly — no level shifter needed.

| GPIO | Function | Notes |
|---|---|---|
| 8 | I2C SDA | BNO055 (0x28); BME680 (0x76, planned) |
| 9 | I2C SCL | |
| 10 | PWMA — Right motor speed | LEDC ch 0, **1 kHz**, 8-bit |
| 11 | AIN1 — Right motor direction A | Motor A = RIGHT |
| 12 | AIN2 — Right motor direction B | |
| 13 | PWMB — Left motor speed | LEDC ch 1, **1 kHz**, 8-bit |
| 14 | BIN1 — Left motor direction A | Motor B = LEFT |
| 15 | BIN2 — Left motor direction B | |
| 17 | (free) | |
| 18 | (free) | |
| 19, 20 | Native USB D−/D+ | micro-ROS transport + flashing → Pi `/dev/ttyACM0` (921600 baud) |
| 39 | Right encoder B | PCNT unit |
| 40 | Left encoder A | PCNT unit ⚠️ 100 nF cap to GND required |
| 41 | Left encoder B | PCNT unit ⚠️ 100 nF cap to GND required |
| 42 | Right encoder A | PCNT unit |
| 43 | UART0 TX via Lonely Binary CH340 | Debug console → Pi `/dev/ttyUSB0` (115200 baud) |
| 44 | UART0 RX via Lonely Binary CH340 | |

### Pins to Avoid

| GPIO | Reason |
|---|---|
| 4, 5, 6, 7 | Not broken out |
| 19, 20 | Native USB CDC — micro-ROS + flashing — do not repurpose |
| 25, 26, 27, 32, 33 | Not broken out |
| 35, 36, 37 | Internal SPI flash — do not use |
| 38 | RGB LED (DevKitC-1); Lonely Binary uses GPIO 48 |
| 43, 44 | UART0 via CH340 — debug console — do not repurpose |
| 0, 45, 46 | Strapping pins |

---

## Encoder Configuration

Encoders use the ESP32 **PCNT** (Pulse Counter) hardware unit via the `ESP32Encoder` library. This is hardware-accelerated and does not use software interrupts.

- `setFilter(400)` rejects pulses shorter than 5 µs, providing hardware glitch rejection against motor switching noise.
- Software EMA is disabled: `VEL_ALPHA = 1.0`. Do not reintroduce EMA — it causes phase lag that destabilizes KD.
- Encoder CPR: **1010 counts per wheel revolution** — validated 2026-05-31 using `/diagnostics/encoder_counts` and a 3 × 1.500 m straight push test. Trials 2 and 3 accepted as the repeatable calibration set: Left 1010.5 / 1008.9 CPR, Right 1009.2 / 1009.2 CPR. Trial 1 right reading of 1020.3 CPR excluded as a directional push outlier. Firmware constant `ENC_CPR_F = 1010.0f`.

⚠️ **GPIO 40/41 EMI:** The left encoder signal path had a bad breadboard section (root cause confirmed by GPIO swap test). This has been corrected. 100 nF ceramic caps from GPIO 40 → GND and GPIO 41 → GND remain required to guard against future switching noise.

---

## Firmware Timing Loops

| Loop | Rate | Tasks |
|---|---|---|
| Control | 100 Hz | Encoder update, PID calculation, PWM output, watchdog check |
| Publish | 30 Hz | IMU poll, odom + IMU publish via micro-ROS |
| Safety | Continuous | cmd_vel watchdog timeout, motors_stop() on loss |

---

## Serial Architecture — Dual Port

| Role | ESP32 | Pi device | Baud | Purpose |
|---|---|---|---|---|
| micro-ROS + flashing | Native USB CDC, GPIO 19/20 (VID 303a:1001) | `/dev/ttyACM0` | 921600 | ROS topics: odom, IMU, cmd_vel; auto-reset flashing |
| Debug console | UART0, GPIO 43/44 via CH340 (VID 1a86:7522) | `/dev/ttyUSB0` | 115200 | Firmware state, PID timing, micro-ROS transitions |

### micro-ROS (ttyACM0)

Publishes:
- `/diff_cont/odom` — `nav_msgs/Odometry` at target 30 Hz — **RELIABLE QoS** (required for fragmentation of ~712-byte messages)
- `/imu/imu` — `sensor_msgs/Imu` at target 30 Hz — **RELIABLE QoS**

Subscribes:
- `/diff_cont/cmd_vel_unstamped` — `geometry_msgs/Twist` — feeds PID targets, resets watchdog

### Debug console (ttyUSB0)

`Serial0.printf()` output, readable with `sudo cat /dev/ttyUSB0`. Example messages:
```
[uROS] CONNECTED + TIME_SYNCED epoch_ms=1748649123456 local_ms=3421
[uROS] ping fail 1/3 t=45678ms
[uROS] agent lost — stopping motors, destroying t=52000ms
[WARN] control dt 1123.00 ms   ← only during time-sync pause on connect
```

Build flag `-DCORE_DEBUG_LEVEL=0` suppresses Arduino framework chatter, keeping the debug stream limited to intentional `Serial0.printf()` output.

---

## Safety Rules

- The watchdog is `WATCHDOG_MS = 500` ms — **validated 2026-05-31** (measured stop time 0.538 s after command loss; criterion ≤0.600 s, PASSED).
- Battery low-voltage cutoff runs on the **Pi** (`battery_publisher` node) — not on the ESP32. The Pi publishes zero cmd_vel and initiates OS shutdown when voltage drops below 9.9V.
- micro-ROS reconnect is fully automatic: pings agent every 2s; 3 consecutive failures → motors stop → entities destroyed → retry every 1s.
- `rmw_uros_sync_session(1000)` is called during connect/reconnect to synchronize ESP32 clock to Pi wall clock. This blocks for up to ~1 second; motors must be stationary during connection.

---

## Required Build Flags

```ini
-DARDUINO_USB_CDC_ON_BOOT=1   ; enables native USB CDC (Serial) for micro-ROS
-DARDUINO_USB_MODE=1          ; uses built-in hardware USB-JTAG/Serial controller
-DCORE_DEBUG_LEVEL=0          ; suppress Arduino framework debug output on UART0
```

---

## Development Notes

- Firmware lives in `firmware/esp32/`.
- Build and flash with PlatformIO: `pio run --target upload` or use `scripts/flash_esp32.sh` (preferred — handles microros-agent service safely).
- **Always stop `microros-agent.service` before flashing** — it grabs `/dev/ttyACM0` and causes mid-write failures.
- Platform pinned at `espressif32@^6.8.0` (arduino-esp32 3.2.x). Do not upgrade without checking LEDC API compatibility.
