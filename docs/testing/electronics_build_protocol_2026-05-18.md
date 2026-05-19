# Electronics Build Protocol — Component-by-Component

**Date:** 2026-05-18
**Purpose:** Add and verify one component at a time. Do not proceed to the next gate until the current gate fully passes. This prevents chasing faults across multiple unknowns.

**Safety rules:**
- Always power off before adding or changing wiring.
- Verify common ground continuity before first power-on at each gate.
- Never connect motor VM (battery voltage) until all logic wiring is confirmed correct.
- Keep a multimeter on hand. Check voltages before connecting sensitive components.
- If anything gets hot unexpectedly: power off immediately.

---

## Gate 1 — Power Hat + Bench Supply

**What you're adding:** RPI5 PD Power Hat P01 connected to bench power supply (or LiPo battery).

**Wiring:**
- Connect DC barrel input to supply set to 12V.
- Leave all outputs disconnected for now.

**Tests:**
1. Power on the supply.
2. Measure voltage at USB-C output port → expect ~5.15V.
3. Measure voltage at VIN screw terminal → expect same as input (~12V).
4. Verify no excessive heat on the hat after 60 seconds idle.

**Pass criteria:**
- [ ] USB-C output: 5.0–5.2V
- [ ] VIN screw terminal: matches input voltage ±0.2V
- [ ] Hat is cool to touch after 60s

**Common failures:**
- No output: check barrel connector polarity (center positive).
- Low voltage: input below hat's minimum (9V). Increase supply voltage.

---

## Gate 2 — Raspberry Pi 5

**What you're adding:** Pi 5 powered from the hat's USB-C output.

**Wiring:**
- USB-C cable: hat USB-C output → Pi USB-C power input.
- Nothing else connected yet.

**Tests:**
1. Power on.
2. Pi should boot to desktop or CLI within ~30 seconds.
3. Connect over SSH or monitor.
4. Run:
```bash
vcgencmd measure_volts core   # expect ~0.9–1.1V
vcgencmd get_throttled        # expect 0x0 — no throttling
```
5. Verify USB-A ports are live (test with a USB flash drive or phone).

**Pass criteria:**
- [ ] Pi boots cleanly — no kernel panics, no rainbow square (undervoltage icon)
- [ ] `get_throttled` returns `0x0`
- [ ] SSH accessible over local network
- [ ] USB-A ports functional

**Common failures:**
- Undervoltage: cable resistance too high — use a short, high-quality USB-C cable rated for 5A.
- No boot: check SD card seated, valid OS image.

---

## Gate 3 — ESP32-S3 (USB, no firmware yet)

**What you're adding:** ESP32-S3-DevKitC-1 on Lonely Binary expansion board, powered from Pi USB-A.

**Wiring:**
- USB-A to USB-C cable: Pi USB-A → ESP32 native USB port (not the UART port).
- No other connections yet — no motors, no sensors.

**Tests:**
1. Power on (Pi already running).
2. On Pi, run:
```bash
ls /dev/ttyACM*          # should show /dev/ttyACM0
ls /dev/serial/by-id/    # should show Espressif entry
dmesg | tail -20         # should show USB CDC ACM device attached
```
3. Verify the ESP32 RGB LED lights up (GPIO 38).

**Pass criteria:**
- [ ] `/dev/ttyACM0` appears on Pi within 5 seconds of plugging in
- [ ] `by-id` path shows `usb-Espressif_USB_JTAG_serial_debug_unit_...`
- [ ] No errors in `dmesg`

**Common failures:**
- Device doesn't appear: wrong USB port on ESP32 (must be native USB, not UART/debug port).
- Permissions denied: add Pi user to `dialout` group: `sudo usermod -a -G dialout $USER`.

---

## Gate 4 — TB6612FNG Motor Driver (Logic Only, No Motors)

**What you're adding:** TB6612FNG breakout, logic power only. No motor VM, no motors connected yet.

**Wiring:**
- TB6612 `VCC` (logic) → ESP32 `3V3` pin
- TB6612 `GND` → common ground rail
- TB6612 `PWMA` → ESP32 GPIO 10
- TB6612 `AIN1` → ESP32 GPIO 11
- TB6612 `AIN2` → ESP32 GPIO 12
- TB6612 `PWMB` → ESP32 GPIO 13
- TB6612 `BIN1` → ESP32 GPIO 14
- TB6612 `BIN2` → ESP32 GPIO 15
- TB6612 `STBY` — **leave unconnected** (Adafruit breakout has onboard 10kΩ pull-up)
- TB6612 `VM` — **leave unconnected** for now
- TB6612 `AO1`, `AO2`, `BO1`, `BO2` — **leave unconnected** for now

**Tests:**
1. Power on.
2. Measure TB6612 `VCC` pin → expect 3.3V.
3. Measure TB6612 `STBY` pin → expect 3.3V (pulled high by onboard resistor).
4. Wiggle GPIO signal wires — multimeter should follow 0/3.3V on AIN1, AIN2, BIN1, BIN2 when toggled from ESP32 (flash a simple test sketch if needed).

**Pass criteria:**
- [ ] TB6612 VCC at 3.3V
- [ ] STBY at 3.3V (motor driver enabled)
- [ ] All 6 signal lines reachable with multimeter, no shorts to ground

**Common failures:**
- STBY low (0V): Adafruit breakout pull-up missing — check board revision, or add external 10kΩ to 3V3.
- VCC shows 5V: wrong power source — TB6612 logic is 3.3V from ESP32, not 5V.

---

## Gate 5 — Motor VM + Right Motor Only

**What you're adding:** Battery/supply VM to TB6612, right motor connected to AO1/AO2.

**Wiring:**
- Battery `+` (or supply ~12V) → TB6612 `VM` via VIN screw terminal on power hat
- Battery `−` → common ground rail
- Right motor terminals → TB6612 `AO1` and `AO2`
- Left motor — **leave disconnected** for now

**Tests:**
Flash a minimal test sketch to ESP32 that:
1. Sets AIN1 HIGH, AIN2 LOW, PWMA to 50% duty.
2. Runs for 2 seconds, then stops.
3. Reverses (AIN1 LOW, AIN2 HIGH) for 2 seconds, then stops.

```
Expected: right motor spins forward, pauses, spins in reverse.
```

Then verify:
- Motor direction is consistent with "Motor A = RIGHT motor" in CLAUDE.md.
- No excessive heat on TB6612 after test.
- Measure VM on TB6612 before connecting motor → should match supply voltage.

**Pass criteria:**
- [ ] VM at expected battery voltage
- [ ] Right motor spins on command in both directions
- [ ] Motor stops cleanly (no coasting/freewheeling when PWMA=0)
- [ ] TB6612 cool after test

**Common failures:**
- Motor doesn't spin: check VM present, check STBY still high after VM applied.
- Motor runs but won't stop: STBY is floating — confirm pull-up is active.
- Wrong direction polarity: note which AIN1/AIN2 state = forward for later PID sign convention.

---

## Gate 6 — Left Motor

**What you're adding:** Left motor connected to TB6612 BO1/BO2.

**Wiring:**
- Left motor terminals → TB6612 `BO1` and `BO2`

**Tests:**
Extend test sketch: repeat Gate 5 test for Motor B (PWMB GPIO 13, BIN1 GPIO 14, BIN2 GPIO 15).

Then run both motors simultaneously:
- Both forward → robot should move straight (or close to it).
- One forward, one reverse → robot should rotate in place.

**Pass criteria:**
- [ ] Left motor spins on command in both directions
- [ ] Both motors running simultaneously: no supply sag, no TB6612 heat
- [ ] Direction polarity noted for both motors (which signal state = forward)

**Common failures:**
- One motor runs backwards relative to the other: swap BO1/BO2 wires, or note the polarity and handle in firmware.
- Supply voltage sags heavily under dual load: supply current limit too low — need ≥3A at 12V for both motors.

---

## Gate 7 — Right Encoder

**What you're adding:** Right encoder wired to ESP32.

**Wiring (JGA25-371 encoder wire colors):**
- Red → motor power (already connected to TB6612/supply)
- White → motor power (already connected)
- Blue → encoder 5V or 3.3V (use 3.3V from ESP32 to keep GPIO-safe)
- Black → encoder GND → common ground
- Yellow (Ch A) → ESP32 GPIO 42
- Green (Ch B) → ESP32 GPIO 39

**Tests:**
Flash a test sketch that attaches `attachInterrupt(GPIO 42, isr, CHANGE)` and counts pulses. Manually rotate the right wheel one full output shaft revolution by hand.

Expected count: **1010 pulses** (ENC_CPR from CLAUDE.md = 1010, at 2× quadrature).

Also verify direction: forward rotation should increment count, reverse should decrement.

```bash
# Or monitor via Serial on Pi:
screen /dev/ttyACM0 115200
# Rotate wheel and watch count
```

**Pass criteria:**
- [ ] One full wheel revolution = 1010 counts ±5%
- [ ] Forward = positive count, reverse = negative
- [ ] No spurious counts when motor is stationary

**Common failures:**
- Count roughly half of expected: only one edge being captured — confirm `CHANGE` mode on interrupt.
- Counts but wrong value: wrong CPR — recount by hand and update `ENC_CPR` if needed.
- Spurious counts at rest: encoder power supply noise — add 100nF cap from encoder VCC to GND.

---

## Gate 8 — Left Encoder (with EMI caps)

**What you're adding:** Left encoder wired to ESP32, with mandatory EMI decoupling caps.

⚠️ **GPIO 40/41 are directly adjacent to PWM motor drive lines and will pick up TB6612 1 kHz PWM noise without hardware mitigation.**

**Wiring:**
- Yellow (Ch A) → 100nF ceramic cap → GND at breadboard, then → ESP32 GPIO 40
- Green (Ch B) → 100nF ceramic cap → GND at breadboard, then → ESP32 GPIO 41
- Blue → 3.3V, Black → GND (same as right encoder)

Route encoder wires away from motor power wires where possible.

**Tests:**
Same as Gate 7 but for left wheel (GPIO 40 interrupt, GPIO 41 read in ISR).

Additional test: run both motors at 50% PWM for 10 seconds while monitoring left encoder count via serial. Count should only change if wheel is physically moving. Spurious counts while stationary = EMI still present.

**Pass criteria:**
- [ ] One full left wheel revolution = 1010 counts ±5%
- [ ] Forward = positive, reverse = negative
- [ ] Zero spurious counts while motors running at 50% PWM and wheel stationary

**Common failures:**
- Spurious counts under motor load: caps not close enough to GPIO — move caps to breadboard directly at GPIO pin, not at encoder connector end.
- Still noisy after caps: try 2× 100nF in parallel (200nF effective), or add a small series resistor (100Ω) before the cap.

---

## Gate 9 — BNO055 IMU

**What you're adding:** BNO055 breakout on I2C bus.

**Wiring:**
- BNO055 `VIN` → ESP32 3.3V
- BNO055 `GND` → common ground
- BNO055 `SDA` → ESP32 GPIO 8
- BNO055 `SCL` → ESP32 GPIO 9
- BNO055 `ADR` → GND (sets I2C address to 0x28)

**Tests:**
1. On Pi, run I2C scan (if I2C is bridged):
```bash
sudo i2cdetect -y 1   # not reliable here since I2C is on ESP32, not Pi
```
2. Flash a test sketch using `Adafruit_BNO055` library:
   - Print accelerometer, gyroscope, and euler angles over serial at 10 Hz.
   - Tilt and rotate the board by hand — values must respond.
   - Calibration status should improve over a minute of movement.

**Pass criteria:**
- [ ] BNO055 initializes without error (`bno.begin()` returns true)
- [ ] Gyro Z reads ~0 rad/s when stationary, responds to rotation
- [ ] Accel reads ~9.8 m/s² on the vertical axis when flat
- [ ] No I2C errors in serial output

**Common failures:**
- `bno.begin()` fails: address wrong — check ADR pin, default is 0x28.
- Garbage data: SDA/SCL swapped — swap GPIO 8 and 9.
- Intermittent errors: I2C pullups missing — add 4.7kΩ from SDA and SCL to 3.3V (Adafruit breakout has onboard pullups, so this is usually not needed).

---

## Gate 10 — INA219 Battery Monitor

**What you're adding:** INA219 breakout on the same I2C bus.

**Wiring:**
- INA219 `VCC` → ESP32 3.3V
- INA219 `GND` → common ground
- INA219 `SDA` → ESP32 GPIO 8 (shared with BNO055)
- INA219 `SCL` → ESP32 GPIO 9 (shared with BNO055)
- INA219 `A0`, `A1` → GND (sets address to 0x40)
- INA219 `VIN+` → battery positive (or supply positive)
- INA219 `VIN-` → downstream load positive (or short to VIN+ for bench test)

**Tests:**
Flash a sketch using `Adafruit_INA219` library:
```
Print: bus voltage, shunt voltage, current mA, power mW
```
With battery connected:
- Bus voltage should read ~12V (or supply voltage).
- Current should read roughly the load draw.

With no load (VIN+ shorted to VIN-): current ≈ 0 mA.

Verify BNO055 still works simultaneously — two devices on same I2C bus.

**Pass criteria:**
- [ ] INA219 initializes without error
- [ ] Bus voltage reads within 0.2V of measured supply voltage
- [ ] Current reads 0 mA ±5 mA with no load
- [ ] BNO055 still reading correctly on same bus

**Common failures:**
- Address conflict: if another device is at 0x40, change A0/A1 jumpers.
- Bus hangs after adding INA219: one device holding SDA low — power cycle and check wiring.

---

## Gate 11 — RPLidar A1

**What you're adding:** RPLidar A1 M8 connected to Raspberry Pi.

**Wiring:**
- USB-A cable: Pi USB-A port → RPLidar USB adapter
- LiDAR power comes from USB — no separate power needed.

**Tests:**
On Pi:
```bash
ls /dev/rplidar          # symlink should exist (set up via udev rule, or use /dev/ttyUSB0)
sudo apt install ros-humble-rplidar-ros  # if not already installed
ros2 run rplidar_ros rplidar_composition --ros-args \
  -p serial_port:=/dev/rplidar -p frame_id:=laser
ros2 topic hz /scan      # expect ~5.5 Hz
ros2 topic echo /scan --once   # check ranges array is populated
```

Slowly wave a hand in front of the LiDAR and watch `/scan` ranges change in RViz2 or echoed output.

**Pass criteria:**
- [ ] `/dev/rplidar` device present
- [ ] `/scan` publishes at ~5.5 Hz
- [ ] `ranges` array shows plausible distances (not all 0 or inf)
- [ ] Moving obstacle visible in scan data

**Common failures:**
- Device not found: no udev rule — use `/dev/ttyUSB0` directly or add udev rule:
  `SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", SYMLINK+="rplidar"`
- Motor doesn't spin: LiDAR motor control signal issue — ensure `rplidar_ros` node is running (it controls motor enable).
- All ranges = 0: motor spinning but laser off — firmware issue, power cycle.

---

## Gate 12 — Intel RealSense D435

**What you're adding:** RealSense D435 connected to Raspberry Pi via USB 3.0.

**Wiring:**
- USB-A (3.0) cable: Pi USB 3.0 port → RealSense D435.
- ⚠️ Must use USB 3.0 port — USB 2.0 bandwidth is insufficient for depth + color at 15 fps.

**Tests:**
On Pi:
```bash
rs-enumerate-devices        # should list D435 serial number and firmware
rs-depth                    # live depth preview (text mode) — optional
ros2 launch realsense2_camera rs_launch.py \
  depth_width:=640 depth_height:=480 depth_fps:=15 \
  color_width:=640 color_height:=480 color_fps:=15
ros2 topic hz /camera/depth/image_rect_raw   # expect ~15 Hz
ros2 topic hz /camera/color/image_raw        # expect ~15 Hz
ros2 topic hz /camera/depth/points           # expect ~15 Hz
```

**Pass criteria:**
- [ ] `rs-enumerate-devices` lists the D435
- [ ] Depth stream at ~15 Hz
- [ ] Color stream at ~15 Hz
- [ ] Point cloud stream at ~15 Hz
- [ ] No USB bandwidth errors in `dmesg`
- [ ] RPLidar still publishing simultaneously (both running at same time)

**Common failures:**
- Low FPS or dropped frames: USB 2.0 port used instead of 3.0 — check `lsusb -t` for SuperSpeed (5000M).
- `rs-enumerate-devices` finds nothing: driver not installed — install `librealsense2` via Intel repo.
- USB errors in dmesg: excessive USB bus load — avoid sharing root hub with other high-bandwidth devices.

---

## Gate 13 — Full Electronics Bench Test (All Components Together)

**What you're testing:** Everything powered simultaneously, all I2C devices, both motor channels, both encoders, LiDAR, RealSense.

**Tests:**
1. Power on full system.
2. Verify all USB devices appear:
```bash
ls /dev/ttyACM0     # ESP32
ls /dev/rplidar     # LiDAR
lsusb | grep Intel  # RealSense
```
3. Flash full firmware to ESP32 (Phase 1 firmware when ready, or current test sketch).
4. Run micro-ROS agent on Pi and verify all topics publish:
```bash
ros2 topic hz /diff_cont/odom
ros2 topic hz /imu/imu
ros2 topic echo /battery_state --once
ros2 topic hz /scan
ros2 topic hz /camera/depth/points
```
5. Send a velocity command and verify robot moves and odom updates.
6. Cut the command stream — verify watchdog stops the motors within 500ms.
7. Monitor supply voltage under full load (all devices + motors running).

**Pass criteria:**
- [ ] All USB devices present simultaneously
- [ ] All ROS topics publishing at expected rates
- [ ] Robot moves on command, odom tracks motion
- [ ] Watchdog stops motors reliably
- [ ] Supply voltage stable (no sag below 10.5V on 12V nominal battery under load)
- [ ] No component overheating after 5-minute run

Gate 13 pass = electronics build complete. Proceed to Phase 2 of `build_plan.md`.

---

## BME680 — Future (Not Yet Wired)

BME680 will be added in Phase 6 (see `build_plan.md`). When ready:
- I2C addr 0x76, same SDA/SCL bus (GPIO 8/9)
- Confirm no address conflict with BNO055 (0x28) and INA219 (0x40) before wiring
- Add a Gate 13b entry and test alongside all other components
