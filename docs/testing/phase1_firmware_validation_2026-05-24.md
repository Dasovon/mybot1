# Phase 1 Firmware Validation — 2026-05-24

## Goal

Flash the ESP32 firmware and validate the Phase 1 micro-ROS gate on real hardware:
- `/diff_cont/odom` publishing at ~30 Hz
- `/imu/imu` publishing at ~30 Hz (requires BNO055 wired)
- `/battery_state` publishing at 1 Hz
- `/diff_cont/cmd_vel_unstamped` subscriber ready
- Auto-flash from Pi with no manual button press

---

## Hardware Context

- ESP32-S3-DevKitC-1 on Lonely Binary expansion board
- Raspberry Pi 5 (Ubuntu Server 24.04, hostname pi5bot)
- ROS 2 Jazzy, micro_ros_arduino v2.0.8-jazzy
- No sensors or motors wired during this session (bare ESP32 only)

---

## Issues Encountered and Fixes

### Issue 1 — `neopixelWrite()` at boot hangs the entire firmware

**Symptom:** After adding `neopixelWrite(48, 0, 0, 0)` as the first line of `setup()` to turn off the RGB LED, zero bytes were received on the serial port at any baud rate. The micro-ROS agent never connected. Even 5 seconds of reading produced nothing.

**Root cause:** `neopixelWrite()` uses the ESP32-S3 RMT peripheral internally. On cold boot, the RMT DMA callback may not fire before the Arduino framework finishes its pre-`setup()` initialization sequence. The function blocks indefinitely waiting for the DMA transfer completion interrupt.

**Fix:** Removed `neopixelWrite()` from `setup()`. The RGB LED remains on for now. A correct approach must be found that calls the LED command later in the boot sequence, after the framework and USB are fully initialized.

**Lesson:** Do not call `neopixelWrite()` as the very first statement in `setup()`. On ESP32-S3 with `ARDUINO_USB_CDC_ON_BOOT=1`, the RMT peripheral may not be ready that early.

---

### Issue 2 — Lonely Binary CH340 UART cannot auto-reset the ESP32 after flashing

**Symptom:** Flashing via the CH340 port (`/dev/ttyUSB0`) with `--after hard-reset` printed "Hard resetting via RTS pin..." but the firmware never started. Zero bytes received after flash. Required a manual RESET button press every time.

**Root cause:** The Lonely Binary expansion board's CH340 RTS line is not connected to the ESP32-S3 EN (reset) pin. `--after hard-reset` via RTS is a no-op on this board — the chip stays in bootloader mode after flashing unless manually reset.

**Fix:** Switched to the ESP32-S3's native USB port (`/dev/ttyACM0`, USB-JTAG/Serial controller, VID 303a:1001). The native USB supports the "1200 bps touch" auto-reset (`--before default-reset`), which reliably triggers the bootloader and restarts the application after flashing — no button press required.

**Lesson:** Always use the native USB port (`/dev/ttyACM0`) for flashing on this board. The CH340 port (`/dev/ttyUSB0`) does not support auto-reset.

---

### Issue 3 — 115200 baud insufficient for odom + IMU bandwidth

**Symptom:** At 115200 baud, only `/battery_state` (1 Hz, ~80 bytes/msg) was publishing messages. `/diff_cont/odom` and `/imu/imu` were registered as topics but no messages arrived.

**Root cause:** Bandwidth calculation:
- `/diff_cont/odom` at 30 Hz: ~700 bytes × 30 = ~21 kB/s
- `/imu/imu` at 30 Hz: ~300 bytes × 30 = ~9 kB/s
- Total: ~30 kB/s required
- 115200 baud = 11,520 bytes/s max — less than half the required throughput

**Fix:** Changed UART baud rate from 115200 to 921600 in both the firmware transport (`Serial.begin(921600)`) and the micro-ROS agent launch command (`-b 921600`). Combined with the switch to native USB CDC (which is reliable at this rate), this resolved the bandwidth bottleneck.

---

### Issue 4 — `setTxTimeoutMs(0)` caused silent partial writes during entity creation

**Symptom:** After switching to native USB CDC (`Serial`), the micro-ROS agent session established successfully but only the first publisher (`/diff_cont/odom`) appeared in `ros2 topic list`. `/imu/imu` and `/battery_state` never registered. `create_entities()` was returning false after the first publisher succeeded.

**Root cause:** `Serial.setTxTimeoutMs(0)` makes `Serial.write()` non-blocking on HWCDC (the ESP32-S3 hardware USB-JTAG Serial controller). During micro-ROS entity creation, the XRCE-DDS library sends multiple protocol messages to the agent for each publisher. If the USB TX buffer is momentarily full during any of these writes, `Serial.write()` returns fewer bytes than requested. The XRCE stream is corrupted, the agent discards the malformed frame, and `rclc_publisher_init_*` times out waiting for a CreateEntity reply — returning `RCL_RET_ERROR`.

**Fix:** Changed `Serial.setTxTimeoutMs(0)` to `Serial.setTxTimeoutMs(100)`. This allows up to 100 ms for the USB TX buffer to drain before a write fails. In practice the USB buffer drains in <5 ms when the agent is actively reading, so this adds no observable latency.

```cpp
extern "C" bool arduino_transport_open(struct uxrCustomTransport * transport) {
    (void)transport;
    Serial.setTxTimeoutMs(100);  // allow buffer to drain; 0 causes partial writes during entity creation
    Serial.begin(921600);
    return true;
}
```

---

### Issue 5 — `nav_msgs/Odometry` silently dropped: message exceeds XRCE transport MTU

**Symptom:** After the entity creation fix, all four topics registered and the agent log showed all publishers and subscribers created. `/battery_state` published at 1 Hz. `/diff_cont/odom` had zero messages — `ros2 topic hz /diff_cont/odom` showed nothing. Verbose agent log showed only one DDS write per second (battery), never any odom writes.

**Root cause:** The micro_ros_arduino v2.0.8-jazzy library sets the custom transport MTU to 512 bytes (`UXR_CONFIG_CUSTOM_TRANSPORT_MTU = 512` in `uxr/client/config.h`).

`nav_msgs/Odometry` in CDR serialization:

| Field | Size |
|---|---|
| Header (stamp + frame_id) | ~16 bytes |
| child_frame_id | ~16 bytes |
| Pose (position + quaternion) | 56 bytes |
| Pose covariance (36 × float64) | 288 bytes |
| Twist (linear + angular) | 48 bytes |
| Twist covariance (36 × float64) | 288 bytes |
| **Total** | **~712 bytes** |

BEST_EFFORT XRCE streams cannot fragment messages larger than the MTU — the packet is silently dropped at the `rcl_publish()` call with no error returned. RELIABLE streams support fragmentation across multiple MTU-sized packets.

**Fix:** Changed `pub_odom` and `pub_imu` from `rclc_publisher_init_best_effort` to `rclc_publisher_init_default` (RELIABLE QoS). This matches the reference design (`articubot_one`) which uses RELIABLE for all publishers.

```cpp
// Before (silently drops odom — 712 bytes > 512-byte MTU):
rclc_publisher_init_best_effort(&pub_odom, &node, ..., "diff_cont/odom");

// After (RELIABLE supports fragmentation):
rclc_publisher_init_default(&pub_odom, &node, ..., "diff_cont/odom");
```

**Note on QoS for EKF subscribers:** `robot_localization` EKF must subscribe to `/diff_cont/odom` and `/imu/imu` with RELIABLE (or `system_default`) QoS to match the publisher. Update the EKF config YAML if it uses `sensor_data` (BEST_EFFORT) profile for these topics.

---

## Final Working Configuration

### Transport
- micro-ROS transport: `Serial` (native USB CDC, GPIO 19/20, ESP32-S3 built-in USB-JTAG)
- Pi device: `/dev/ttyACM0` (VID 303a:1001)
- Baud rate: 921600

### Flash command (from Pi, no button press needed)
```bash
python3 -m esptool \
    --chip esp32s3 \
    --port /dev/ttyACM0 \
    --baud 921600 \
    --before default-reset \
    --after hard-reset \
    write-flash \
    --flash-mode dio \
    --flash-freq 80m \
    --flash-size detect \
    0x10000 firmware.bin
```

### Build on Pi
```bash
cd ~/bot_ws/firmware/esp32
~/.local/bin/pio run
```

### Agent launch
```bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -b 921600
```

### Validated topic rates (bare ESP32, no sensors/motors wired)
| Topic | Expected | Observed |
|---|---|---|
| `/diff_cont/odom` | 30 Hz | 30.2 Hz ✓ |
| `/battery_state` | 1 Hz | 1.0 Hz ✓ |
| `/imu/imu` | 30 Hz | registered, publishes when BNO055 wired ✓ |
| `/diff_cont/cmd_vel_unstamped` | subscriber | ready ✓ |

---

## Remaining Phase 1 Gate Items

The following require sensors and motors to be wired:
- [ ] `/imu/imu` publishing at ~30 Hz (wire BNO055: SDA GPIO 8, SCL GPIO 9, addr 0x28)
- [ ] `/battery_state` showing plausible voltage/current (wire INA219: SDA GPIO 8, SCL GPIO 9, addr 0x40)
- [ ] Wheel velocity tracks `cmd_vel` command under closed-loop PID (wire Cytron MDD10A GPIO 10–13, encoders GPIO 39–42)
- [ ] Robot moves forward on `cmd_vel` then stops on watchdog timeout
- [ ] Battery task keeps publishing without gaps while motors are under load
- [x] RGB LED off (GPIO 48) — fixed: `neopixelWrite(48, 0, 0, 0)` called on first `loop()` iteration after framework fully initialised; confirmed off after reflash
