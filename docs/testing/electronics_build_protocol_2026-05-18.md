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

## Software Prerequisites

Install these on your **development PC** before starting. The Pi and ESP32 software is covered gate-by-gate below.

| Tool | Purpose | Install |
|---|---|---|
| VS Code | Editor and PlatformIO host | https://code.visualstudio.com/ |
| PlatformIO IDE | ESP32 firmware build + flash | https://platformio.org/install/ide?install=vscode |
| Git | Version control | https://git-scm.com/ |

---

## Power & Ground Wiring Principles

These principles apply at every gate. Read them once before starting.

### Common ground — non-negotiable
Battery negative, power hat GND, Pi GND, ESP32 GND, TB6612 GND, and every sensor GND share one ground bus. Run each component's ground wire directly to that bus — never daisy-chain grounds through another component.

### Motor power vs logic power — keep them separate
Motor current is high, switching, and noisy. Logic current is low and clean. The two rails share only the ground bus:
- **Motor power path**: Battery → hat VIN screw terminal → TB6612 VM. Heavy gauge wire. No logic components on this path.
- **Logic power path**: Battery → hat USB-C → Pi 5V → USB-A → ESP32 3.3V → sensors. Light gauge, clean rail.
Do not route motor power and logic power through the same wire or terminal.

### Decoupling capacitors — place as you add each component
- **TB6612 VM/GND**: 100µF electrolytic + 100nF ceramic in parallel, close to the VM and GND pins.
- **Each I2C sensor VCC**: 100nF ceramic close to the sensor's VCC pin.
- **GPIO 40/41 (left encoder only)**: 100nF ceramic from each GPIO line to GND, placed on the breadboard as close to the ESP32 pin as possible. These GPIOs pick up 1 kHz motor PWM noise and caps are not optional.

### Cable routing
- Motor power cables and signal cables (I2C, encoder, USB) must travel separately.
- Keep encoder signal wires short and away from motor wiring.
- Keep I2C wires under 30 cm — long wires add capacitance and cause reliability issues at 400 kHz.

### Measure before connecting
At each gate: power on and measure voltage at the new component's power pin before making any signal connections. Correct voltage → proceed. Wrong voltage → stop and trace back.

---

## Gate 1 — Power Hat + Bench Supply

**What you're adding:** RPI5 PD Power Hat P01 connected to bench power supply (or LiPo battery).

**Software used:** None — multimeter only.

**Hardware reference:** [`docs/hardware/rpi5_pd_power_hat.md`](../hardware/rpi5_pd_power_hat.md)

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

**Hardware reference:** [`docs/hardware/raspberry_pi_5.md`](../hardware/raspberry_pi_5.md)

### Software setup

**Step 1 — Flash Raspberry Pi OS**

Download and install Raspberry Pi Imager on your dev PC:
https://www.raspberrypi.com/software/

In the Imager:
1. Choose device: **Raspberry Pi 5**
2. Choose OS: **Raspberry Pi OS (64-bit)** — the full desktop or lite version
3. Choose storage: your microSD card (≥32GB recommended)
4. Click the gear icon (⚙) before writing and configure:
   - Set hostname (e.g. `mybot`)
   - Enable SSH
   - Set username and password
   - Configure Wi-Fi (your network SSID + password)
5. Write the image

**Step 2 — First boot and SSH**

Insert the SD card into the Pi. Connect power. Wait ~60 seconds for first boot.

Find the Pi's IP address (check your router, or use):
```bash
# On your dev PC:
ping mybot.local       # if mDNS works on your network
# or
nmap -sn 192.168.1.0/24 | grep -i raspberry
```

Connect over SSH:
```bash
ssh ryan@mybot.local   # use the username you set in Imager
```

**Step 3 — Update the system**
```bash
sudo apt update && sudo apt full-upgrade -y
sudo reboot
```

**Step 4 — Install ROS 2 Humble on Pi**

ROS 2 Humble is required on the Pi for the micro-ROS agent and sensor drivers. Full installation guide: https://docs.ros.org/en/humble/Installation/Ubuntu-Install-Debians.html

Quick install:
```bash
# Locale
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

# Add ROS 2 apt repo
sudo apt install software-properties-common curl -y
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
  http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
  | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

# Install ROS 2 base (use base, not desktop — Pi doesn't need RViz2)
sudo apt update && sudo apt install ros-humble-ros-base -y
sudo apt install python3-rosdep python3-colcon-common-extensions -y

# Source ROS 2 on every login
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
source ~/.bashrc

# Initialize rosdep
sudo rosdep init
rosdep update
```

**Step 5 — Build micro-ROS agent from source (arm64)**

The micro-ROS agent is not available as an apt package for arm64. Build it from source in `~/microros_ws`:

```bash
source /opt/ros/humble/setup.bash
mkdir -p ~/microros_ws/src && cd ~/microros_ws

# Clone micro-ROS setup tools
git clone -b humble https://github.com/micro-ROS/micro_ros_setup.git src/micro_ros_setup

# Install dependencies
rosdep update
rosdep install --from-paths src --ignore-src -y

# Build setup tools
colcon build && source install/local_setup.bash

# Create and build the agent workspace
ros2 run micro_ros_setup create_agent_ws.sh
ros2 run micro_ros_setup build_agent.sh
source install/local_setup.bash
```

Add to `~/.bashrc` so it loads on every login:
```bash
echo "source ~/microros_ws/install/local_setup.bash" >> ~/.bashrc
```

**Tests:**
```bash
vcgencmd measure_volts core   # expect ~0.9–1.1V
vcgencmd get_throttled        # expect 0x0 — no throttling
```

Verify USB-A ports are live (test with a USB flash drive or phone).

**Pass criteria:**
- [ ] Pi boots cleanly — no kernel panics, no rainbow square (undervoltage icon)
- [ ] `get_throttled` returns `0x0`
- [ ] SSH accessible over local network
- [ ] ROS 2 Humble installed: `ros2 --version` returns without error
- [ ] micro-ROS agent built: `ros2 run micro_ros_agent micro_ros_agent --help` works
- [ ] USB-A ports functional

**Common failures:**
- Undervoltage: cable resistance too high — use a short, high-quality USB-C cable rated for 5A.
- No boot: check SD card seated, valid OS image.
- ROS 2 install fails: wrong Ubuntu version — Pi must be running Ubuntu 22.04 or Raspberry Pi OS Bookworm (based on Debian 12).

---

## Gate 3 — ESP32-S3 (USB, no firmware yet)

**What you're adding:** ESP32-S3-DevKitC-1 on Lonely Binary expansion board, powered from Pi USB-A.

**Hardware reference:** [`docs/hardware/esp32_s3.md`](../hardware/esp32_s3.md)

### Software setup

**Step 1 — Install PlatformIO on dev PC**

1. Install VS Code: https://code.visualstudio.com/
2. Open VS Code → Extensions → search **PlatformIO IDE** → Install
3. Restart VS Code. The PlatformIO home screen should open.

**Step 2 — Create the firmware project**

In PlatformIO Home → New Project:
- Name: `esp32_firmware`
- Board: `Espressif ESP32-S3-DevKitC-1`
- Framework: `Arduino`
- Location: point to `firmware/esp32/` in this repo

This creates `platformio.ini`. Add the required build flags:
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
monitor_speed = 115200
upload_protocol = esptool
```

**Step 3 — Flash a blink sketch to confirm toolchain works**

In `firmware/esp32/src/main.cpp`:
```cpp
#include <Arduino.h>
void setup() { pinMode(38, OUTPUT); }   // GPIO 38 = onboard RGB LED
void loop() { digitalWrite(38, HIGH); delay(500); digitalWrite(38, LOW); delay(500); }
```

In VS Code: click the PlatformIO **Upload** button (→ arrow in the bottom toolbar). If it asks for a port, select the ESP32's USB device.

⚠️ Use the **native USB port** on the ESP32, not the UART/debug port — check the silkscreen label.

**Tests on Pi:**
```bash
ls /dev/ttyACM*          # should show /dev/ttyACM0
ls /dev/serial/by-id/    # should show Espressif entry
dmesg | tail -20         # should show USB CDC ACM device attached

# Add Pi user to dialout group (run once, then re-login):
sudo usermod -a -G dialout $USER
```

**Monitor serial output from Pi:**
```bash
screen /dev/ttyACM0 115200
# Press Ctrl+A then K to exit screen
```

**Pass criteria:**
- [ ] `/dev/ttyACM0` appears on Pi within 5 seconds of plugging in
- [ ] `by-id` path shows `usb-Espressif_USB_JTAG_serial_debug_unit_...`
- [ ] No errors in `dmesg`
- [ ] PlatformIO uploads blink sketch without error
- [ ] ESP32 RGB LED blinks

**Common failures:**
- Device doesn't appear: wrong USB port on ESP32 (must be native USB, not UART/debug port).
- Upload fails with "No serial port": check USB cable supports data (not charge-only).
- `ARDUINO_USB_CDC_ON_BOOT` warning: build flag missing from `platformio.ini` — add it.

---

## Gate 4 — TB6612FNG Motor Driver (Logic Only, No Motors)

**What you're adding:** TB6612FNG breakout, logic power only. No motor VM, no motors connected yet.

**Hardware reference:** [`docs/hardware/tb6612fng.md`](../hardware/tb6612fng.md)

### Software setup

**Test sketch** — paste into `firmware/esp32/src/main.cpp` and upload via PlatformIO:

```cpp
#include <Arduino.h>

#define AIN1 11
#define AIN2 12
#define BIN1 14
#define BIN2 15
#define PWMA 10
#define PWMB 13

void setup() {
    Serial.begin(115200);
    pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
    pinMode(PWMA, OUTPUT); pinMode(PWMB, OUTPUT);

    // Toggle all lines so you can probe with multimeter
    digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
    analogWrite(PWMA, 128);   // ~50% duty
    analogWrite(PWMB, 128);
    Serial.println("TB6612 logic test running");
}

void loop() {}
```

Upload with PlatformIO. Monitor via:
```bash
screen /dev/ttyACM0 115200
```

**Tests:**
1. Measure TB6612 `VCC` pin → expect 3.3V.
2. Measure TB6612 `STBY` pin → expect 3.3V (pulled high by onboard resistor).
3. With sketch running, probe AIN1 → expect 3.3V. Probe AIN2 → expect 0V.
4. Probe PWMA → should read ~1.65V average (50% duty at 3.3V) on a multimeter, or toggle clearly on an oscilloscope.

**Pass criteria:**
- [ ] TB6612 VCC at 3.3V
- [ ] STBY at 3.3V (motor driver enabled)
- [ ] AIN1 reads HIGH, AIN2 reads LOW with sketch running
- [ ] `Serial.println` output visible on serial monitor

**Common failures:**
- STBY low (0V): Adafruit breakout pull-up missing — check board revision, or add external 10kΩ to 3V3.
- VCC shows 5V: wrong power source — TB6612 logic is 3.3V from ESP32, not 5V.
- `analogWrite` doesn't compile: ESP32 Arduino core uses `ledcWrite` — update sketch if needed.

---

## Gate 5 — Motor VM + Right Motor Only

**What you're adding:** Battery/supply VM to TB6612, right motor connected to AO1/AO2.

**Hardware reference:** [`docs/hardware/tb6612fng.md`](../hardware/tb6612fng.md)

### Software setup

**Test sketch** — upload via PlatformIO:

```cpp
#include <Arduino.h>
#include "driver/ledc.h"

#define AIN1 11
#define AIN2 12
#define PWMA 10
#define PWM_FREQ   1000
#define PWM_RES    8
#define PWM_CH     0

void setup() {
    Serial.begin(115200);
    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    ledcSetup(PWM_CH, PWM_FREQ, PWM_RES);
    ledcAttachPin(PWMA, PWM_CH);
}

void loop() {
    // Forward at 50%
    Serial.println("Forward");
    digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
    ledcWrite(PWM_CH, 128);
    delay(2000);

    // Stop
    Serial.println("Stop");
    ledcWrite(PWM_CH, 0);
    delay(500);

    // Reverse at 50%
    Serial.println("Reverse");
    digitalWrite(AIN1, LOW); digitalWrite(AIN2, HIGH);
    ledcWrite(PWM_CH, 128);
    delay(2000);

    // Stop
    ledcWrite(PWM_CH, 0);
    delay(500);
}
```

⚠️ Measure VM on TB6612 before connecting the motor — it must match supply voltage.

**Tests:**
1. Upload sketch. Open serial monitor (`screen /dev/ttyACM0 115200`).
2. Right motor should spin forward 2s, stop, reverse 2s, stop, repeat.
3. Note which AIN1/AIN2 state produces forward vs reverse — record it.
4. Touch TB6612 after 1 minute — should not be hot.

**Pass criteria:**
- [ ] VM at expected battery voltage before motor connected
- [ ] Right motor spins on command in both directions
- [ ] Motor stops cleanly when `ledcWrite(PWM_CH, 0)`
- [ ] TB6612 cool after 1-minute run
- [ ] Motor direction polarity recorded

**Common failures:**
- Motor doesn't spin: check VM present, check STBY still high after VM applied.
- Motor runs but won't stop: STBY is floating — confirm pull-up is active.
- Motor runs backwards from expected: swap AO1/AO2 wires at the motor connector.

---

## Gate 6 — Left Motor

**What you're adding:** Left motor connected to TB6612 BO1/BO2.

**Hardware reference:** [`docs/hardware/tb6612fng.md`](../hardware/tb6612fng.md)

### Software setup

Extend the Gate 5 sketch to also drive Motor B:

```cpp
#include <Arduino.h>
#include "driver/ledc.h"

#define AIN1 11  #define AIN2 12  #define PWMA 10
#define BIN1 14  #define BIN2 15  #define PWMB 13
#define PWM_FREQ 1000  #define PWM_RES 8
#define PWM_CHA 0      #define PWM_CHB 1

void setup() {
    Serial.begin(115200);
    pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
    ledcSetup(PWM_CHA, PWM_FREQ, PWM_RES); ledcAttachPin(PWMA, PWM_CHA);
    ledcSetup(PWM_CHB, PWM_FREQ, PWM_RES); ledcAttachPin(PWMB, PWM_CHB);
}

void driveRight(int pwm, bool fwd) {
    digitalWrite(AIN1, fwd); digitalWrite(AIN2, !fwd);
    ledcWrite(PWM_CHA, pwm);
}
void driveLeft(int pwm, bool fwd) {
    digitalWrite(BIN1, fwd); digitalWrite(BIN2, !fwd);
    ledcWrite(PWM_CHB, pwm);
}
void stopAll() { ledcWrite(PWM_CHA, 0); ledcWrite(PWM_CHB, 0); }

void loop() {
    Serial.println("Both forward");
    driveRight(128, true); driveLeft(128, true); delay(2000); stopAll(); delay(500);

    Serial.println("Rotate in place");
    driveRight(128, true); driveLeft(128, false); delay(2000); stopAll(); delay(500);
}
```

**Tests:**
1. Both motors forward → robot moves straight (or close to it).
2. One forward, one reverse → rotates in place.
3. Monitor supply voltage under dual load — should not sag below 10.5V.

**Pass criteria:**
- [ ] Left motor spins on command in both directions
- [ ] Both motors simultaneously: no supply sag, no TB6612 heat
- [ ] Direction polarity noted for both motors

**Common failures:**
- One motor runs backwards relative to the other: swap BO1/BO2 wires at motor connector.
- Supply sags heavily: current limit too low — need ≥3A at 12V.

---

## Gate 7 — Right Encoder

**What you're adding:** Right encoder wired to ESP32.

**Hardware reference:** [`docs/hardware/wheel_encoders.md`](../hardware/wheel_encoders.md)

### Software setup

**Test sketch:**

```cpp
#include <Arduino.h>

#define ENC_R_A 42
#define ENC_R_B 39

volatile long countR = 0;

void IRAM_ATTR isrRightA() {
    countR += (digitalRead(ENC_R_B) == digitalRead(ENC_R_A)) ? -1 : 1;
}

void setup() {
    Serial.begin(115200);
    pinMode(ENC_R_A, INPUT_PULLUP);
    pinMode(ENC_R_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENC_R_A), isrRightA, CHANGE);
}

void loop() {
    Serial.print("Right encoder count: ");
    Serial.println(countR);
    delay(100);
}
```

Upload via PlatformIO. Monitor on Pi:
```bash
screen /dev/ttyACM0 115200
```

**Tests:**
1. Rotate right wheel one full revolution by hand. Count must reach **1010 ±50**.
2. Rotate in opposite direction — count must decrement.
3. Hold wheel stationary — count must not drift.

**Pass criteria:**
- [ ] One full revolution = 1010 counts ±5%
- [ ] Forward = positive, reverse = negative
- [ ] No spurious counts when motor is stationary

**Common failures:**
- Count ~505 (half expected): only one edge captured — confirm `CHANGE` mode.
- Count ~2020 (double): counting both A and B transitions — only interrupt on A.
- Spurious counts at rest: add 100nF cap from encoder VCC to GND.

---

## Gate 8 — Left Encoder (with EMI caps)

**What you're adding:** Left encoder wired to ESP32 with mandatory EMI decoupling caps.

**Hardware reference:** [`docs/hardware/wheel_encoders.md`](../hardware/wheel_encoders.md)

⚠️ **GPIO 40/41 pick up TB6612 1 kHz PWM noise. Caps are not optional.**

### Software setup

Extend the Gate 7 sketch to add the left encoder and apply an EMA filter:

```cpp
#include <Arduino.h>

#define ENC_R_A 42  #define ENC_R_B 39
#define ENC_L_A 40  #define ENC_L_B 41
#define VEL_ALPHA 0.2f

volatile long countR = 0, countL = 0;
volatile long lastCountL = 0;
float velL_filtered = 0;

void IRAM_ATTR isrRightA() {
    countR += (digitalRead(ENC_R_B) == digitalRead(ENC_R_A)) ? -1 : 1;
}
void IRAM_ATTR isrLeftA() {
    countL += (digitalRead(ENC_L_B) == digitalRead(ENC_L_A)) ? -1 : 1;
}

void setup() {
    Serial.begin(115200);
    pinMode(ENC_R_A, INPUT_PULLUP); pinMode(ENC_R_B, INPUT_PULLUP);
    pinMode(ENC_L_A, INPUT_PULLUP); pinMode(ENC_L_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(ENC_R_A), isrRightA, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENC_L_A), isrLeftA, CHANGE);
}

void loop() {
    long rawDelta = countL - lastCountL;
    lastCountL = countL;
    velL_filtered = VEL_ALPHA * rawDelta + (1.0f - VEL_ALPHA) * velL_filtered;

    Serial.print("R:"); Serial.print(countR);
    Serial.print("  L:"); Serial.print(countL);
    Serial.print("  L_vel_filtered:"); Serial.println(velL_filtered);
    delay(100);
}
```

**Tests:**
1. Rotate left wheel one full revolution by hand → count = 1010 ±50.
2. Run both motors at 50% PWM, wheels lifted off ground. Watch `countL` via serial — must not drift while wheels are stationary.
3. `velL_filtered` should read ~0 when stationary under motor load.

**Pass criteria:**
- [ ] One full left revolution = 1010 counts ±5%
- [ ] Forward = positive, reverse = negative
- [ ] Zero spurious counts while motors running at 50% PWM and wheel stationary

**Common failures:**
- Spurious counts under motor load: move caps to breadboard as close to GPIO pin as possible.
- Still noisy: use 2× 100nF in parallel (200nF), or add 100Ω series resistor before cap.

---

## Gate 9 — BNO055 IMU

**What you're adding:** BNO055 breakout on I2C bus.

**Hardware reference:** [`docs/hardware/bno055_imu.md`](../hardware/bno055_imu.md)

### Software setup

**Step 1 — Add libraries to `platformio.ini`:**
```ini
lib_deps =
    adafruit/Adafruit BNO055 @ ^1.6.3
    adafruit/Adafruit Unified Sensor @ ^1.1.9
```

Library sources:
- Adafruit BNO055: https://github.com/adafruit/Adafruit_BNO055
- Adafruit Unified Sensor: https://github.com/adafruit/Adafruit_Sensor

PlatformIO will download these automatically on next build.

**Step 2 — Test sketch:**

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>

Adafruit_BNO055 bno(55, 0x28, &Wire);

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);   // SDA=GPIO8, SCL=GPIO9
    if (!bno.begin()) {
        Serial.println("BNO055 not found — check wiring and address");
        while (1);
    }
    bno.setExtCrystalUse(true);
    Serial.println("BNO055 ready");
}

void loop() {
    sensors_event_t event;
    bno.getEvent(&event);

    imu::Vector<3> gyro = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
    imu::Vector<3> accel = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);

    Serial.print("Gyro Z: "); Serial.print(gyro.z());
    Serial.print("  Accel X: "); Serial.print(accel.x());
    Serial.print("  Y: "); Serial.print(accel.y());
    Serial.print("  Z: "); Serial.println(accel.z());
    delay(100);
}
```

**Tests:**
1. Upload sketch. Open serial monitor.
2. `BNO055 ready` must print — if not, init failed.
3. Hold flat: accel Z ≈ 9.8, X and Y ≈ 0.
4. Rotate board: Gyro Z changes. Stop: returns to ~0.
5. Tilt 90°: accel axis shifts accordingly.

**Pass criteria:**
- [ ] `bno.begin()` succeeds
- [ ] Gyro Z reads ~0 rad/s stationary, responds to rotation
- [ ] Accel reads ~9.8 m/s² on vertical axis when flat
- [ ] No I2C errors or `not found` messages

**Common failures:**
- Init fails: ADR pin floating — tie it to GND explicitly.
- Garbage values: SDA/SCL swapped — try swapping GPIO 8 and 9.
- Intermittent: long wire capacitance — shorten I2C wires or reduce I2C clock speed with `Wire.setClock(100000)`.

---

## Gate 10 — INA219 Battery Monitor

**What you're adding:** INA219 breakout on the same I2C bus.

**Hardware reference:** [`docs/hardware/ina219_battery_monitor.md`](../hardware/ina219_battery_monitor.md)

### Software setup

**Step 1 — Add library to `platformio.ini`:**
```ini
lib_deps =
    adafruit/Adafruit BNO055 @ ^1.6.3
    adafruit/Adafruit Unified Sensor @ ^1.1.9
    adafruit/Adafruit INA219 @ ^1.2.1
```

Library source: https://github.com/adafruit/Adafruit_INA219

**Step 2 — Test sketch (reads both BNO055 and INA219 simultaneously):**

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_INA219.h>

Adafruit_BNO055 bno(55, 0x28, &Wire);
Adafruit_INA219 ina(0x40);

void setup() {
    Serial.begin(115200);
    Wire.begin(8, 9);
    if (!bno.begin()) { Serial.println("BNO055 FAIL"); while(1); }
    if (!ina.begin())  { Serial.println("INA219 FAIL"); while(1); }
    Serial.println("Both sensors ready");
}

void loop() {
    float busV   = ina.getBusVoltage_V();
    float current = ina.getCurrent_mA();
    imu::Vector<3> gyro = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);

    Serial.print("Bus V: "); Serial.print(busV);
    Serial.print("V  Current: "); Serial.print(current);
    Serial.print("mA  Gyro Z: "); Serial.println(gyro.z());
    delay(500);
}
```

**Tests:**
1. Upload sketch. Both `ready` messages must appear.
2. Bus voltage must read within 0.2V of measured supply voltage.
3. With VIN+ shorted to VIN-: current ≈ 0 mA.
4. BNO055 gyro still responds to movement.

**Pass criteria:**
- [ ] INA219 initializes without error
- [ ] Bus voltage within 0.2V of supply
- [ ] Current reads 0 mA ±5 mA with no load
- [ ] BNO055 still reading correctly on shared I2C bus

**Common failures:**
- INA219 FAIL: address conflict — check A0/A1 jumpers, verify 0x40 not taken.
- Bus hangs: one device pulling SDA low — power cycle and check wiring order.
- Wildly wrong current: shunt resistor value mismatch — Adafruit INA219 uses 0.1Ω, library defaults to this.

---

## Gate 11 — RPLidar A1

**What you're adding:** RPLidar A1 M8 connected to Raspberry Pi.

**Hardware reference:** [`docs/hardware/rplidar.md`](../hardware/rplidar.md)

### Software setup

**Step 1 — Install rplidar ROS 2 package on Pi:**
```bash
sudo apt install ros-humble-rplidar-ros -y
```

**Step 2 — Set up udev rule so device always appears as `/dev/rplidar`:**
```bash
# Find the vendor ID first:
lsusb   # look for Silicon Labs CP210x — vendor ID 10c4

# Create udev rule:
sudo tee /etc/udev/rules.d/99-rplidar.rules > /dev/null << 'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", SYMLINK+="rplidar", MODE="0666"
EOF

sudo udevadm control --reload-rules && sudo udevadm trigger
```

Unplug and replug the LiDAR — `/dev/rplidar` should appear.

**Step 3 — Test launch:**
```bash
source /opt/ros/humble/setup.bash
ros2 run rplidar_ros rplidar_composition --ros-args \
    -p serial_port:=/dev/rplidar \
    -p frame_id:=laser \
    -p angle_compensate:=true
```

In a second terminal:
```bash
source /opt/ros/humble/setup.bash
ros2 topic hz /scan
ros2 topic echo /scan --once | head -20
```

**Optional — Visualize in RViz2 (on dev PC):**

Install RViz2 on dev PC:
```bash
sudo apt install ros-humble-desktop -y   # includes RViz2
```

Set `ROS_DOMAIN_ID` to match Pi, or use a shared network and same ROS 2 environment. Then:
```bash
rviz2
# Add → By topic → /scan → LaserScan
```

**Pass criteria:**
- [ ] `/dev/rplidar` appears after udev rule
- [ ] `/scan` publishes at ~5.5 Hz
- [ ] `ranges` array populated with non-zero, non-inf values
- [ ] Moving obstacle visible in scan

**Common failures:**
- Device not found: wrong vendor ID — check `lsusb` for your specific cable/adapter.
- Motor doesn't spin: `rplidar_ros` node not running (it controls motor enable via DTR line).
- All ranges = 0 or inf: motor spinning but laser off — power cycle LiDAR.

---

## Gate 12 — Intel RealSense D435

**What you're adding:** RealSense D435 connected to Raspberry Pi via USB 3.0.

**Hardware reference:** [`docs/hardware/realsense_d435.md`](../hardware/realsense_d435.md)

### Software setup

**Step 1 — Install librealsense2 on Pi**

Intel RealSense SDK (librealsense2) source: https://github.com/IntelRealSense/librealsense

For Raspberry Pi (arm64), install from Intel's apt server:
```bash
# Add Intel RealSense apt key and repo
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-key F6E65AC044F831AC80A06380C8B3A55A6F3EFCD \
  || sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-key F6E65AC044F831AC80A06380C8B3A55A6F3EFCD

sudo add-apt-repository "deb https://librealsense.intel.com/Debian/apt-repo $(lsb_release -cs) main" -u
sudo apt install librealsense2-utils librealsense2-dev -y
```

Verify install:
```bash
rs-enumerate-devices   # should list D435 serial number and firmware version
```

**Step 2 — Install realsense2_camera ROS package**

ROS wrapper source: https://github.com/IntelRealSense/realsense-ros

Install via apt:
```bash
sudo apt install ros-humble-realsense2-camera ros-humble-realsense2-description -y
```

**Step 3 — Test launch:**
```bash
source /opt/ros/humble/setup.bash
ros2 launch realsense2_camera rs_launch.py \
    depth_width:=640 depth_height:=480 depth_fps:=15 \
    color_width:=640 color_height:=480 color_fps:=15 \
    pointcloud.enable:=true
```

In separate terminals:
```bash
ros2 topic hz /camera/depth/image_rect_raw
ros2 topic hz /camera/color/image_raw
ros2 topic hz /camera/depth/points
```

Check USB is SuperSpeed (USB 3.0):
```bash
lsusb -t   # look for D435 at 5000M — if it shows 480M it's USB 2.0
```

⚠️ RealSense requires a USB 3.0 port — USB 2.0 bandwidth is insufficient.

**Pass criteria:**
- [ ] `rs-enumerate-devices` lists D435
- [ ] Depth stream at ~15 Hz
- [ ] Color stream at ~15 Hz
- [ ] Point cloud at ~15 Hz
- [ ] `lsusb -t` shows 5000M (USB 3.0 SuperSpeed)
- [ ] No USB errors in `dmesg`
- [ ] RPLidar still publishing at ~5.5 Hz simultaneously

**Common failures:**
- Low FPS / dropped frames: USB 2.0 port — check port physically and `lsusb -t`.
- `rs-enumerate-devices` empty: librealsense2 not installed, or wrong backend — try `--backend-type libuvc`.
- USB errors in dmesg: too many high-bandwidth devices on same root hub — move to different physical port.

---

## Gate 13 — Waveshare 2.42" OLED Display

**What you're adding:** Waveshare 2.42inch OLED Module (SSD1309, 128×64) connected to Raspberry Pi via SPI0.

**Hardware reference:** [`docs/hardware/oled_display.md`](../hardware/oled_display.md)

### Software setup

**Step 1 — Enable SPI on the Pi:**
```bash
sudo raspi-config
# → Interface Options → SPI → Yes → Finish → reboot
```

Verify after reboot:
```bash
ls /dev/spidev*   # must show /dev/spidev0.0
```

**Step 2 — Install Python dependencies:**
```bash
sudo apt update
sudo apt install python3-pip python3-pil python3-spidev python3-smbus -y
pip3 install lgpio
```

> Use `lgpio` — not bcm2835 or WiringPi, which do not work on Raspberry Pi 5 / Ubuntu 22.04.

**Step 3 — Download Waveshare demo code:**

Library source: https://github.com/waveshare/2.42inch-OLED-Module

```bash
git clone https://github.com/waveshare/2.42inch-OLED-Module.git ~/oled_demo
cd ~/oled_demo/RaspberryPi/python/
```

**Step 4 — Run the test script:**
```bash
sudo python3 OLED_2in42_test.py
```

The display must cycle through: text, shapes, and a logo image.

⚠️ DC pin is board pin 22. Board pin 21 (same row, adjacent column) is MISO — wrong pin = blank display with no error.

**Tests:**
1. Power off Pi. Wire the OLED to Pi SPI0 — pin functions in `docs/hardware/oled_display.md`.
2. Power on Pi — verify no smoke, display connector is not hot.
3. Run `OLED_2in42_test.py` — demo must complete all frames.
4. Verify all prior USB devices still enumerate (`ls /dev/ttyACM0`, `/dev/rplidar`, `lsusb | grep Intel`).

**Pass criteria:**
- [ ] `/dev/spidev0.0` present after reboot
- [ ] OLED demo runs: text and shapes visible on display
- [ ] Pi still reachable over SSH during and after display test
- [ ] ESP32 and LiDAR still present on their device paths simultaneously

**Common failures:**

| Symptom | Likely Cause |
|---|---|
| Blank display, no error | DC or RES on wrong pin — verify board pin 22 vs 21 |
| `/dev/spidev0.0` missing | SPI not enabled — re-run `raspi-config` and reboot |
| `ImportError: lgpio` | Run `pip3 install lgpio` |
| Partial / garbled image | VCC is 5V through a resistor that's too high — use Pi 3.3V pin directly |

---

## Gate 14 — Full Electronics Bench Test

**What you're testing:** All components powered simultaneously. All sensors streaming. Robot moves on command. Watchdog stops it safely.

### Software: micro-ROS agent

The micro-ROS agent was built in Gate 2. Run it on Pi:
```bash
source /opt/ros/humble/setup.bash
source ~/microros_ws/install/local_setup.bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -b 115200
```

For the full bench test, the ESP32 must be running the Phase 1 firmware from `build_plan.md`. If Phase 1 firmware isn't written yet, use the combined test sketch from Gates 4–8 and verify manually.

### Full system check

**Step 1 — Verify all USB devices present:**
```bash
ls /dev/ttyACM0        # ESP32
ls /dev/rplidar        # LiDAR
lsusb | grep Intel     # RealSense
```

**Step 2 — Launch all sensors:**
```bash
# Terminal 1 — micro-ROS agent
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0

# Terminal 2 — RPLidar
ros2 run rplidar_ros rplidar_composition --ros-args -p serial_port:=/dev/rplidar -p frame_id:=laser

# Terminal 3 — RealSense
ros2 launch realsense2_camera rs_launch.py depth_width:=640 depth_height:=480 \
    depth_fps:=15 color_width:=640 color_height:=480 color_fps:=15 pointcloud.enable:=true
```

**Step 3 — Verify all topics:**
```bash
# Terminal 4
source /opt/ros/humble/setup.bash
ros2 topic hz /diff_cont/odom          # ~30 Hz
ros2 topic hz /imu/imu                  # ~30 Hz
ros2 topic echo /battery_state --once  # voltage/current values
ros2 topic hz /scan                     # ~5.5 Hz
ros2 topic hz /camera/depth/points      # ~15 Hz
```

**Step 4 — Drive test and watchdog:**
```bash
# Send a velocity command (robot must be on the floor or wheels lifted):
ros2 topic pub /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist \
    "{linear: {x: 0.1, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.0}}" --rate 10

# Ctrl+C to stop — robot must stop within 500ms (watchdog timeout)
```

**Step 5 — 5-minute soak test:**
Run all nodes for 5 minutes with periodic motion commands. Monitor:
```bash
watch -n 2 "ros2 topic hz /diff_cont/odom /scan /camera/depth/points --window 20 2>&1 | tail -15"
```

**Pass criteria:**
- [ ] All USB devices present simultaneously
- [ ] `/diff_cont/odom` at ~30 Hz
- [ ] `/imu/imu` at ~30 Hz
- [ ] `/battery_state` publishes with plausible voltage and current
- [ ] `/scan` at ~5.5 Hz
- [ ] `/camera/depth/points` at ~15 Hz
- [ ] Robot moves on velocity command, odom updates
- [ ] Watchdog stops motors within 500ms of command stream cut
- [ ] Supply voltage stable (no sag below 10.5V under full load)
- [ ] No component overheating after 5-minute run

**Gate 14 pass = electronics build complete. Proceed to Phase 2 of [`build_plan.md`](../architecture/build_plan.md).**

---

## BME680 — Future (Not Yet Wired)

BME680 will be added in Phase 6. When ready:
- I2C addr 0x76, same SDA/SCL bus (GPIO 8/9)
- Library: https://github.com/adafruit/Adafruit_BME680
- Add to `platformio.ini`: `adafruit/Adafruit BME680 Library`
- Confirm no address conflict with BNO055 (0x28) and INA219 (0x40) before wiring
- Follow the same test-first approach as Gates 9–10
