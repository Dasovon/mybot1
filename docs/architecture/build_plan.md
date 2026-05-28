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
| 1 — ESP32 Firmware | **Complete** — all gate checks pass. IMU 30 Hz, encoders correct direction, motors forward, watchdog fires. PID gains need tuning before Phase 2 odom accuracy matters. See [`docs/testing/phase1_motor_validation_2026-05-25.md`](../testing/phase1_motor_validation_2026-05-25.md) |
| 2 — ROS 2 Foundation (URDF + TF) | **Complete** — 9 frames, all named correctly, no disconnected frames. base_footprint root added. JSP Jazzy 2.4.1 invocation fixed. Sensor offsets are placeholders — measure before Phase 3 EKF. See [`docs/testing/phase2_urdf_tf_validation_2026-05-27.md`](../testing/phase2_urdf_tf_validation_2026-05-27.md) |
| 3 — Sensor Bridge & EKF | Not started |
| 4 — SLAM | Not started |
| 5 — Nav2 Autonomous Navigation | Not started |
| 6 — Extended Sensors | Not started |
| 7 — Semantic Perception | Not started |

Update the table above as each phase completes.

---

## Phase 0 — Hardware & Environment

Add and verify one component at a time. Do not proceed to the next step until the current step fully passes. This prevents chasing faults across multiple unknowns.

**Safety rules:**
- Always power off before adding or changing wiring.
- Verify common ground continuity before first power-on at each step.
- Never connect motor VM (battery voltage) until all logic wiring is confirmed correct.
- Keep a multimeter on hand. Check voltages before connecting sensitive components.
- If anything gets hot unexpectedly: power off immediately.

---

### Software Prerequisites

Install these on your **development PC** before starting. Pi and ESP32 software is covered step-by-step below.

| Tool | Purpose | Install |
|---|---|---|
| VS Code | Editor and PlatformIO host | https://code.visualstudio.com/ |
| PlatformIO IDE | ESP32 firmware build + flash | https://platformio.org/install/ide?install=vscode |
| Git | Version control | https://git-scm.com/ |

---

### Power & Ground Wiring Principles

These principles apply at every step. Read them once before starting.

#### Common ground — non-negotiable
Battery negative, power hat GND, Pi GND, ESP32 GND, TB6612 GND, and every sensor GND share one ground bus. Run each component's ground wire directly to that bus — never daisy-chain grounds through another component.

#### Motor power vs logic power — keep them separate
Motor current is high, switching, and noisy. Logic current is low and clean. The two rails share only the ground bus:
- **Motor power path**: Battery → hat VIN screw terminal → TB6612 VM. Heavy gauge wire. No logic components on this path.
- **Logic power path**: Battery → hat USB-C → Pi 5V → USB-A → ESP32 3.3V → sensors. Light gauge, clean rail.

Do not route motor power and logic power through the same wire or terminal.

#### Decoupling capacitors — place as you add each component
- **TB6612 VM/GND**: 100µF electrolytic + 100nF ceramic in parallel, close to the VM and GND pins.
- **Each I2C sensor VCC**: 100nF ceramic close to the sensor's VCC pin.
- **GPIO 40/41 (left encoder only)**: 100nF ceramic from each GPIO line to GND, placed on the breadboard as close to the ESP32 pin as possible. These GPIOs pick up 20 kHz motor PWM switching noise and caps are not optional.

#### Cable routing
- Motor power cables and signal cables (I2C, encoder, USB) must travel separately.
- Keep encoder signal wires short and away from motor wiring.
- Keep I2C wires under 30 cm — long wires add capacitance and cause reliability issues at 400 kHz.

#### Measure before connecting
At each step: power on and measure voltage at the new component's power pin before making any signal connections. Correct voltage → proceed. Wrong voltage → stop and trace back.

---

### Step 1 — Power Hat + Bench Supply

**What you're adding:** EP-0225 (52pi) power hat connected to bench power supply (or LiPo battery).

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

### Step 2 — Raspberry Pi 5

**What you're adding:** Pi 5 powered from the hat's USB-C output.

**Hardware reference:** [`docs/hardware/raspberry_pi_5.md`](../hardware/raspberry_pi_5.md)

**2.1 — Flash Ubuntu Server 24.04 LTS**

> OS: **Ubuntu Server 24.04 LTS (64-bit)** — Tier 1 for ROS 2 Jazzy, standard apt packages, no workarounds needed.
> ⚠️ **Do NOT use Imager customization.** Setting hostname/user/SSH/WiFi in the Imager triggers cloud-init provisioning which fails intermittently on Pi 5. Flash with no customization and configure manually after first boot.

In Raspberry Pi Imager:
1. Choose device: **Raspberry Pi 5**
2. Choose OS: **Other general-purpose OS → Ubuntu → Ubuntu Server 24.04 LTS (64-bit)**
3. Choose storage: your microSD card (≥32GB recommended)
4. When asked about customization: **No / Skip**
5. Write the image and let verification finish

**2.2 — First boot and SSH**

Insert the SD card into the Pi. Connect **Ethernet only** — no USB peripherals yet. Power on and wait **5–10 minutes** for first boot (cloud-init runs on first boot even with no customization).

Find the Pi's IP (check Google Wifi app or your router's DHCP table, or):
```bash
nmap -sn 192.168.86.0/24 | grep -i ubuntu
```

Clear any stale host key from previous flashes:
```bash
ssh-keygen -f "/home/ryan/.ssh/known_hosts" -R "<PI_IP>"
```

Connect — default credentials are `ubuntu` / `ubuntu`:
```bash
ssh ubuntu@<PI_IP>
# You will be forced to change the password immediately
```

After password change, set hostname:
```bash
sudo hostnamectl set-hostname pi5bot
sudo sed -i 's/127.0.1.1.*/127.0.1.1 pi5bot/' /etc/hosts
sudo reboot
```

After reboot, SSH using the new hostname:
```bash
ssh ubuntu@pi5bot.local
```

**2.3 — Update the system**
```bash
sudo apt update && sudo apt full-upgrade -y
sudo reboot
```

**2.3a — Enable full USB port power**

The Pi 5 limits USB ports to 600mA if it doesn't detect a 5V/5A supply. Third-party power hats may not advertise 5A in a way the Pi recognizes. RPLidar peaks at ~500mA and RealSense at ~900mA — both are at risk without this flag.

```bash
echo "[all]" | sudo tee -a /boot/firmware/config.txt
echo "usb_max_current_enable=1" | sudo tee -a /boot/firmware/config.txt
sudo reboot
```

After reboot verify no undervoltage events:
```bash
vcgencmd get_throttled   # must return 0x0
```

**2.4 — Install ROS 2 Jazzy on Pi**

Install base only — the Pi does not run RViz2.

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

sudo apt update && sudo apt install ros-jazzy-ros-base -y
sudo apt install python3-rosdep python3-colcon-common-extensions -y

echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
source ~/.bashrc

sudo rosdep init
rosdep update
```

**2.5 — Build micro-ROS agent from source (arm64)**

Not available as an apt package for arm64. Build from source in `~/microros_ws`:

```bash
source /opt/ros/jazzy/setup.bash
mkdir -p ~/microros_ws/src && cd ~/microros_ws

git clone -b jazzy https://github.com/micro-ROS/micro_ros_setup.git src/micro_ros_setup

rosdep update
rosdep install --from-paths src --ignore-src -y

colcon build && source install/local_setup.bash

ros2 run micro_ros_setup create_agent_ws.sh
ros2 run micro_ros_setup build_agent.sh
source install/local_setup.bash
```

Add to `~/.bashrc`:
```bash
echo "source ~/microros_ws/install/local_setup.bash" >> ~/.bashrc
```

**Tests:**
```bash
vcgencmd measure_volts core   # expect ~0.9–1.1V
vcgencmd get_throttled        # expect 0x0 — no throttling
```

**Pass criteria:**
- [ ] Pi boots cleanly — no kernel panics, no rainbow square (undervoltage icon)
- [ ] `get_throttled` returns `0x0`
- [ ] SSH accessible over local network
- [ ] ROS 2 Jazzy installed: `ros2 --version` returns without error
- [ ] micro-ROS agent built: `ros2 run micro_ros_agent micro_ros_agent --help` works
- [ ] USB-A ports functional

**Common failures:**
- Undervoltage: cable resistance too high — use a short, high-quality USB-C cable rated for 5A.
- No boot: check SD card seated, valid OS image.
- ROS 2 install fails: verify `$UBUNTU_CODENAME` resolves to `noble` (`cat /etc/os-release | grep CODENAME`). If not, the wrong Ubuntu image was flashed.

---

### Step 3 — ESP32-S3 (USB, no firmware yet)

**What you're adding:** ESP32-S3-DevKitC-1 on Lonely Binary expansion board, powered from Pi USB-A.

**Hardware reference:** [`docs/hardware/esp32_s3.md`](../hardware/esp32_s3.md)

**3.1 — Install PlatformIO on dev PC**

1. Install VS Code: https://code.visualstudio.com/
2. Open VS Code → Extensions → search **PlatformIO IDE** → Install
3. Restart VS Code.

**3.2 — Create the firmware project**

In PlatformIO Home → New Project:
- Name: `esp32_firmware`
- Board: `Espressif ESP32-S3-DevKitC-1`
- Framework: `Arduino`
- Location: `firmware/esp32/` in this repo

Add the required build flags to `platformio.ini`:
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32@^6.8.0
board = esp32-s3-devkitc-1
framework = arduino
build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
monitor_speed = 115200
upload_protocol = esptool
```

> ⚠️ **Platform version must be pinned.** Two known constraints:
> - **Minimum ≥ 6.3.0** — earlier versions have an I2C clock-stretching bug that makes BNO055 unreliable (ESP-IDF < 5.4.0).
> - **arduino-esp32 3.3.6+ has a UART regression** — `Serial1.begin()` with custom GPIO pins (our GPIO 17/18 for micro-ROS) silently fails. As of May 2026, avoid the latest PlatformIO espressif32 release if it bundles arduino-esp32 ≥ 3.3.6. `espressif32@^6.8.0` pins to a known-good range. If micro-ROS stops responding after a PlatformIO update, check `pio pkg show espressif32` and downgrade if arduino-esp32 is 3.3.6+.
>
> Verify the arduino-esp32 version bundled: `pio pkg show espressif32 | grep arduino-esp32`

**3.3 — Flash a blink sketch to confirm toolchain**

In `firmware/esp32/src/main.cpp`:
```cpp
#include <Arduino.h>
void setup() { pinMode(38, OUTPUT); }   // GPIO 38 = onboard RGB LED
void loop() { digitalWrite(38, HIGH); delay(500); digitalWrite(38, LOW); delay(500); }
```

Click the PlatformIO **Upload** button. ⚠️ Use the **native USB port**, not the UART/debug port.

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
- `ARDUINO_USB_CDC_ON_BOOT` warning: build flag missing from `platformio.ini`.

---

### Step 4 — TB6612FNG Motor Driver (Logic Only, No Motors)

**What you're adding:** TB6612FNG breakout, logic power only. No motor VM, no motors connected yet.

**Hardware reference:** [`docs/hardware/tb6612fng.md`](../hardware/tb6612fng.md)

**Test sketch** — paste into `firmware/esp32/src/main.cpp` and upload:

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
    digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
    ledcSetup(0, 20000, 8); ledcAttachPin(PWMA, 0);
    ledcSetup(1, 20000, 8); ledcAttachPin(PWMB, 1);
    ledcWrite(0, 128);
    ledcWrite(1, 128);
    Serial.println("TB6612 logic test running");
}

void loop() {}
```

**Tests:**
1. Measure TB6612 `VCC` pin → expect 3.3V.
2. Measure TB6612 `STBY` pin → expect 3.3V (pulled high by onboard resistor).
3. Probe AIN1 → expect 3.3V. Probe AIN2 → expect 0V.
4. Probe PWMA → ~1.65V average (50% duty) on multimeter.

**Pass criteria:**
- [ ] TB6612 VCC at 3.3V
- [ ] STBY at 3.3V (motor driver enabled)
- [ ] AIN1 reads HIGH, AIN2 reads LOW with sketch running
- [ ] `Serial.println` output visible on serial monitor

**Common failures:**
- STBY low (0V): Adafruit breakout pull-up missing — add external 10kΩ to 3V3.
- VCC shows 5V: wrong power source — TB6612 logic is 3.3V from ESP32.
- PWMA/PWMB not toggling: confirm `ledcSetup` and `ledcAttachPin` ran before `ledcWrite`.

---

### Step 5 — Motor VM + Right Motor Only

**What you're adding:** Battery/supply VM to TB6612, right motor connected to AO1/AO2.

**Hardware reference:** [`docs/hardware/tb6612fng.md`](../hardware/tb6612fng.md)

**Test sketch:**

```cpp
#include <Arduino.h>
#include "driver/ledc.h"

#define AIN1 11
#define AIN2 12
#define PWMA 10
#define PWM_FREQ   20000
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
    Serial.println("Forward");
    digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
    ledcWrite(PWM_CH, 128);
    delay(2000);

    Serial.println("Stop");
    ledcWrite(PWM_CH, 0);
    delay(500);

    Serial.println("Reverse");
    digitalWrite(AIN1, LOW); digitalWrite(AIN2, HIGH);
    ledcWrite(PWM_CH, 128);
    delay(2000);

    ledcWrite(PWM_CH, 0);
    delay(500);
}
```

⚠️ Measure VM on TB6612 before connecting the motor — it must match supply voltage.

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

### Step 6 — Left Motor

**What you're adding:** Left motor connected to TB6612 BO1/BO2.

**Hardware reference:** [`docs/hardware/tb6612fng.md`](../hardware/tb6612fng.md)

Extend the Step 5 sketch to drive Motor B:

```cpp
#include <Arduino.h>
#include "driver/ledc.h"

#define AIN1 11  #define AIN2 12  #define PWMA 10
#define BIN1 14  #define BIN2 15  #define PWMB 13
#define PWM_FREQ 20000  #define PWM_RES 8
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

**Pass criteria:**
- [ ] Left motor spins on command in both directions
- [ ] Both motors simultaneously: no supply sag, no TB6612 heat
- [ ] Direction polarity noted for both motors

**Common failures:**
- One motor runs backwards relative to the other: swap BO1/BO2 wires at motor connector.
- Supply sags heavily: current limit too low — need ≥3A at 12V.

---

### Step 7 — Right Encoder

**What you're adding:** Right encoder wired to ESP32.

**Hardware reference:** [`docs/hardware/wheel_encoders.md`](../hardware/wheel_encoders.md)

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

### Step 8 — Left Encoder (with EMI caps)

**What you're adding:** Left encoder wired to ESP32 with mandatory EMI decoupling caps.

**Hardware reference:** [`docs/hardware/wheel_encoders.md`](../hardware/wheel_encoders.md)

⚠️ **GPIO 40/41 pick up TB6612 20 kHz PWM switching noise. Caps are not optional.**

Extend the Step 7 sketch to add the left encoder with EMA filter:

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
2. Run both motors at 50% PWM, wheels lifted off ground. Watch `countL` — must not drift.
3. `velL_filtered` should read ~0 when stationary under motor load.

**Pass criteria:**
- [ ] One full left revolution = 1010 counts ±5%
- [ ] Forward = positive, reverse = negative
- [ ] Zero spurious counts while motors running at 50% PWM and wheel stationary

**Common failures:**
- Spurious counts under motor load: move caps to breadboard as close to GPIO pin as possible.
- Still noisy: use 2× 100nF in parallel (200nF), or add 100Ω series resistor before cap.

---

### Step 9 — BNO055 IMU

**What you're adding:** BNO055 breakout on I2C bus.

**Hardware reference:** [`docs/hardware/bno055_imu.md`](../hardware/bno055_imu.md)

**9.1 — Add libraries to `platformio.ini`:**
```ini
lib_deps =
    adafruit/Adafruit BNO055 @ ^1.6.3
    adafruit/Adafruit Unified Sensor @ ^1.1.9
```

**9.2 — Test sketch:**

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
1. `BNO055 ready` must print — if not, init failed.
2. Hold flat: accel Z ≈ 9.8, X and Y ≈ 0.
3. Rotate board: Gyro Z changes. Stop: returns to ~0.

**Pass criteria:**
- [ ] `bno.begin()` succeeds
- [ ] Gyro Z reads ~0 rad/s stationary, responds to rotation
- [ ] Accel reads ~9.8 m/s² on vertical axis when flat
- [ ] No I2C errors or `not found` messages

**Common failures:**
- Init fails: ADR pin floating — tie it to GND explicitly.
- Garbage values: SDA/SCL swapped — try swapping GPIO 8 and 9.
- Intermittent: shorten I2C wires or add `Wire.setClock(100000)`.

---

### Step 10 — INA219 Battery Monitor

**What you're adding:** INA219 breakout on the same I2C bus.

**Hardware reference:** [`docs/hardware/ina219_battery_monitor.md`](../hardware/ina219_battery_monitor.md)

**10.1 — Add library to `platformio.ini`:**
```ini
lib_deps =
    adafruit/Adafruit BNO055 @ ^1.6.3
    adafruit/Adafruit Unified Sensor @ ^1.1.9
    adafruit/Adafruit INA219 @ ^1.2.1
```

**10.2 — Test sketch (reads both sensors simultaneously):**

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
    float busV    = ina.getBusVoltage_V();
    float current = ina.getCurrent_mA();
    imu::Vector<3> gyro = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
    Serial.print("Bus V: "); Serial.print(busV);
    Serial.print("V  Current: "); Serial.print(current);
    Serial.print("mA  Gyro Z: "); Serial.println(gyro.z());
    delay(500);
}
```

**Pass criteria:**
- [ ] INA219 initializes without error
- [ ] Bus voltage within 0.2V of supply
- [ ] Current reads 0 mA ±5 mA with no load
- [ ] BNO055 still reading correctly on shared I2C bus

**Common failures:**
- INA219 FAIL: address conflict — check A0/A1 jumpers, verify 0x40 not taken.
- Bus hangs: one device pulling SDA low — power cycle and check wiring order.
- Wildly wrong current: Adafruit INA219 uses 0.1Ω shunt — library defaults to this.

---

### Step 11 — RPLidar A1

**What you're adding:** RPLidar A1 M8 connected to Raspberry Pi.

**Hardware reference:** [`docs/hardware/rplidar.md`](../hardware/rplidar.md)

**11.1 — Install rplidar ROS 2 package on Pi:**
```bash
sudo apt install ros-jazzy-rplidar-ros -y
```

**11.2 — Set up udev rule:**
```bash
lsusb   # look for Silicon Labs CP210x — vendor ID 10c4

sudo tee /etc/udev/rules.d/99-rplidar.rules > /dev/null << 'EOF'
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", SYMLINK+="rplidar", MODE="0666"
EOF

sudo udevadm control --reload-rules && sudo udevadm trigger
```

Unplug and replug the LiDAR — `/dev/rplidar` should appear.

**11.3 — Test launch:**
```bash
source /opt/ros/jazzy/setup.bash
ros2 run rplidar_ros rplidar_composition --ros-args \
    -p serial_port:=/dev/rplidar \
    -p frame_id:=laser \
    -p angle_compensate:=true
```

In a second terminal:
```bash
ros2 topic hz /scan
ros2 topic echo /scan --once | head -20
```

**Optional — Visualize in RViz2 on dev PC:**
```bash
sudo apt install ros-jazzy-desktop -y
rviz2
# Add → By topic → /scan → LaserScan
```

**Pass criteria:**
- [ ] `/dev/rplidar` appears after udev rule
- [ ] `/scan` publishes at ~5.5 Hz
- [ ] `ranges` array populated with non-zero, non-inf values
- [ ] Moving obstacle visible in scan

**Common failures:**
- Device not found: wrong vendor ID — check `lsusb` for your specific adapter.
- Motor doesn't spin: `rplidar_ros` node not running (it controls motor enable via DTR).
- All ranges = 0 or inf: motor spinning but laser off — power cycle LiDAR.

---

### Step 12 — Intel RealSense D435

**What you're adding:** RealSense D435 connected to Raspberry Pi via USB 3.0.

**Hardware reference:** [`docs/hardware/realsense_d435.md`](../hardware/realsense_d435.md)

**12.1 — Install librealsense2 on Pi:**
```bash
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-key F6E65AC044F831AC80A06380C8B3A55A6F3EFCD \
  || sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-key F6E65AC044F831AC80A06380C8B3A55A6F3EFCD

sudo add-apt-repository "deb https://librealsense.intel.com/Debian/apt-repo $(lsb_release -cs) main" -u
sudo apt install librealsense2-utils librealsense2-dev -y
```

Verify:
```bash
rs-enumerate-devices   # should list D435 serial number and firmware version
```

**12.2 — Install realsense2_camera ROS package:**
```bash
sudo apt install ros-jazzy-realsense2-camera ros-jazzy-realsense2-description -y
```

**12.3 — Test launch:**
```bash
source /opt/ros/jazzy/setup.bash
ros2 launch realsense2_camera rs_launch.py \
    depth_width:=640 depth_height:=480 depth_fps:=15 \
    color_width:=640 color_height:=480 color_fps:=15 \
    pointcloud.enable:=true
```

Check USB is SuperSpeed:
```bash
lsusb -t   # D435 must show 5000M — if 480M it's USB 2.0
```

⚠️ RealSense requires USB 3.0 — USB 2.0 bandwidth is insufficient.

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
- `rs-enumerate-devices` empty: try `--backend-type libuvc`.
- USB errors in dmesg: too many high-bandwidth devices on same root hub — move to different port.

---

### Step 13 — Waveshare 2.42" OLED Display

**What you're adding:** Waveshare 2.42inch OLED Module (SSD1309, 128×64) connected to Raspberry Pi via SPI0.

**Hardware reference:** [`docs/hardware/oled_display.md`](../hardware/oled_display.md)

**13.1 — Enable SPI on the Pi:**
```bash
sudo raspi-config
# → Interface Options → SPI → Yes → Finish → reboot
```

Verify after reboot:
```bash
ls /dev/spidev*   # must show /dev/spidev0.0
```

**13.2 — Install Python dependencies:**
```bash
sudo apt update
sudo apt install python3-pip python3-pil python3-spidev python3-smbus -y
pip3 install lgpio
```

> Use `lgpio` — not bcm2835 or WiringPi, which do not work on Raspberry Pi 5.

**13.3 — Download Waveshare demo code:**
```bash
git clone https://github.com/waveshare/2.42inch-OLED-Module.git ~/oled_demo
cd ~/oled_demo/RaspberryPi/python/
```

**13.4 — Run the test script:**
```bash
sudo python3 OLED_2in42_test.py
```

The display must cycle through text, shapes, and a logo image.

⚠️ DC pin is board pin 22. Board pin 21 (adjacent) is MISO — wrong pin = blank display with no error.

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
| Partial / garbled image | VCC too high — use Pi 3.3V pin directly |

---

### Step 14 — Full Electronics Bench Test

**What you're testing:** All components powered simultaneously. All sensors streaming. Robot moves on command. Watchdog stops it safely.

**Prerequisite — start micro-ROS agent on Pi** (built in Step 2):
```bash
source /opt/ros/jazzy/setup.bash
source ~/microros_ws/install/local_setup.bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyUSB0 -b 115200
```

For the full bench test, the ESP32 must be running Phase 1 firmware. If Phase 1 firmware isn't written yet, use the combined test sketch from Steps 4–8 and verify manually.

**14.1 — Verify all USB devices present:**
```bash
ls /dev/ttyACM0        # ESP32
ls /dev/rplidar        # LiDAR
lsusb | grep Intel     # RealSense
```

**14.2 — Launch all sensors:**
```bash
# Terminal 1 — micro-ROS agent
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyUSB0 -b 115200

# Terminal 2 — RPLidar
ros2 run rplidar_ros rplidar_composition --ros-args -p serial_port:=/dev/rplidar -p frame_id:=laser

# Terminal 3 — RealSense
ros2 launch realsense2_camera rs_launch.py depth_width:=640 depth_height:=480 \
    depth_fps:=15 color_width:=640 color_height:=480 color_fps:=15 pointcloud.enable:=true
```

**14.3 — Verify all topics:**
```bash
ros2 topic hz /diff_cont/odom          # ~30 Hz
ros2 topic hz /imu/imu                  # ~30 Hz
ros2 topic echo /battery_state --once  # voltage/current values
ros2 topic hz /scan                     # ~5.5 Hz
ros2 topic hz /camera/depth/points      # ~15 Hz
```

**14.4 — Drive test and watchdog:**
```bash
ros2 topic pub /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist \
    "{linear: {x: 0.1, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.0}}" --rate 10
# Ctrl+C to stop — robot must stop within 500ms (watchdog timeout)
```

**14.5 — 5-minute soak test:**
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

**Step 14 pass = Phase 0 complete. Proceed to Phase 1.**

**Phase 0 complete when:**
- All hardware wired per GPIO map in CLAUDE.md
- GPIO 40/41 EMI caps confirmed working (Step 8)
- Common ground verified across all components
- Pi reachable over SSH, `/dev/ttyACM0` present, `/dev/rplidar` present
- OLED display working on Pi SPI0 (Step 13)
- All ROS topics publishing at expected rates with full system powered (Step 14)

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

**Step 1 — PlatformIO project**
Create `firmware/esp32/platformio.ini` targeting `esp32-s3-devkitc-1`. Required build flags:
```
-DARDUINO_USB_CDC_ON_BOOT=1            ; enables Serial0 USB CDC for display telemetry
-DMICRO_ROS_TRANSPORT_ARDUINO_SERIAL   ; micro-ROS uses Serial1 hardware UART
```
Dependencies: `micro_ros_arduino`, `Adafruit BNO055`, `Adafruit INA219`, `Wire`.

Serial assignment (must be set before any other Serial init):
```cpp
// Serial0 = native USB CDC (GPIO 19/20) → display telemetry JSON to Pi
// Serial1 = UART (GPIO 17 TX, GPIO 18 RX) → micro-ROS to Pi via USB-UART adapter
Serial.begin(115200);                              // Serial0: display telemetry
Serial1.begin(115200, SERIAL_8N1, 18, 17);         // Serial1: micro-ROS
set_microros_serial_transports(Serial1);
```

**Step 2 — Motor driver**
Implement TB6612 control using ESP32 LEDC peripheral. GPIO map:
- PWMA (GPIO 10) → right motor, LEDC ch 0, **20 kHz**, 8-bit
- AIN1/AIN2 (GPIO 11/12) → right direction
- PWMB (GPIO 13) → left motor, LEDC ch 1, **20 kHz**, 8-bit
- BIN1/BIN2 (GPIO 14/15) → left direction

Expose: `motors_set_velocity(float right_mps, float left_mps)` and `motors_stop()`.

**Step 3 — Encoder ISR**
Attach interrupts on GPIO 42 (right A) and GPIO 40 (left A) as CHANGE. Read B channels (GPIO 39, 41) inside ISR for direction. Constants from CLAUDE.md: `ENC_CPR = 1010`, `wheel_radius = 0.033 m`.

Apply EMA filter on left encoder velocity (`VEL_ALPHA = 0.2`) to suppress GPIO 40/41 PWM noise.

> **Recommended upgrade:** The ESP32-S3 PCNT (Pulse Counter) peripheral provides hardware quadrature decoding with a built-in glitch filter — no ISR, no EMA filter, no CPU overhead. Use the [ESP32Encoder](https://github.com/madhephaestus/ESP32Encoder) library (`madhephaestus/ESP32Encoder`). Replace the `attachInterrupt` + EMA approach with PCNT in Phase 1 firmware for cleaner, lower-jitter encoder counts. The EMA approach in the test sketch above is still valid for Phase 0 bench verification.

**Step 4 — PID controller**
One PID instance per wheel. Input: measured wheel velocity (rad/s). Output: PWM command. Run at 100 Hz in a FreeRTOS task or `loop()`. Expose tunable Kp, Ki, Kd constants via `#define` in a header.

**Step 5 — IMU (BNO055)**
Initialize on I2C (GPIO 8 SDA, GPIO 9 SCL, addr 0x28). Read linear acceleration and angular velocity at 30 Hz. Do not use magnetometer (unreliable on metal chassis).

**Step 6 — Battery monitor (INA219)**
Initialize on same I2C bus (addr 0x40). Read bus voltage and current at 1 Hz.

**The INA219 read and `/battery_state` publish must run in their own dedicated FreeRTOS task** — not inside the motor/PID loop or the micro-ROS spin loop. This is a hard requirement learned from a prior build: when battery monitoring shares execution context with motor control, running a motor test (or any blocking operation) pauses battery updates. The battery task must be able to read and publish at 1 Hz regardless of what the motor, encoder, or IMU tasks are doing.

Implementation rule: create a `battery_task` pinned to **core 0** with its own `vTaskDelay(pdMS_TO_TICKS(1000))` cadence. Pin the PID/motor task to **core 1** (where `loop()` runs). Never call the INA219 read from `loop()` or from within the PID task.

**Step 7 — Safety watchdog**
If no `/diff_cont/cmd_vel_unstamped` message is received within 500 ms, call `motors_stop()`. Watchdog must run independently of micro-ROS connection state — use a hardware timer or FreeRTOS timer, not a ROS callback.

**Step 8 — micro-ROS node**
Transport: Serial1 UART (GPIO 17 TX / GPIO 18 RX) via USB-UART adapter to Pi `/dev/ttyUSB0`. Publishers:
- `/diff_cont/odom` — `nav_msgs/Odometry` at 30 Hz
- `/imu/imu` — `sensor_msgs/Imu` at 30 Hz
- `/battery_state` — `sensor_msgs/BatteryState` at 1 Hz

Subscriber:
- `/diff_cont/cmd_vel_unstamped` — `geometry_msgs/msg/Twist` — feed velocity targets to PID, reset watchdog

Frame IDs: `odom` → `base_link` for odometry, `imu_link` for IMU.

### Validation gate — Phase 1

Run on Pi with micro-ROS agent active:
```bash
ros2 topic hz /diff_cont/odom      # must be ~30 Hz
ros2 topic hz /imu/imu             # must be ~30 Hz
ros2 topic echo /battery_state --once   # must show plausible voltage/current
ros2 topic pub /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist \
  "{linear: {x: 0.1}, angular: {z: 0.0}}" --once
# robot should move forward briefly, then stop after watchdog timeout
```

**Battery isolation check (required):** While the motors are actively spinning under a continuous velocity command, verify `/battery_state` keeps publishing without gaps:
```bash
# Terminal 1 — continuous drive command
ros2 topic pub /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist \
  "{linear: {x: 0.2}, angular: {z: 0.0}}" --rate 20

# Terminal 2 — battery must not skip beats during motor load
ros2 topic hz /battery_state --window 30   # must hold ~1 Hz, no dropouts
```

If `/battery_state` drops out or pauses while motors run, the battery task is sharing execution context with the motor/PID loop — fix the FreeRTOS task structure before proceeding.

Phase 1 is complete when all checks above pass, including the battery isolation check.

**Session log (2026-05-24):** micro-ROS transport, baud rate, and QoS issues resolved. odom at 30 Hz and battery at 1 Hz confirmed on bare hardware. Sensors/motors not yet wired. See [`docs/testing/phase1_firmware_validation_2026-05-24.md`](../testing/phase1_firmware_validation_2026-05-24.md) for the full issue log and transport configuration details.

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

**Step 1 — Chassis and wheels**
Define `base_link` as the robot body. Add `left_wheel` and `right_wheel` as continuous joints. Use measured values from CLAUDE.md: `wheel_radius = 0.033 m`, `wheel_separation = 0.177 m`.

**Step 2 — Sensor frames**
Define static frames relative to `base_link`:
- `laser` — RPLidar A1 mount position
- `imu_link` — BNO055 position (on ESP32 board)
- `camera_link` — RealSense D435 mount position
- `camera_depth_frame` — child of `camera_link`

Frame positions must match physical sensor placement on the robot. Measure and record them.

**Step 3 — Diff drive plugin**
Add the `ros2_control` diff drive controller configured for:
- Left joint: `left_wheel`
- Right joint: `right_wheel`
- Wheel separation: 0.177 m
- Wheel radius: 0.033 m
- Command topic: `/diff_cont/cmd_vel_unstamped`
- Odometry topic: `/diff_cont/odom`

**Step 4 — Launch file**
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
| `src/robot_bringup/config/ekf.yaml` | robot_localization EKF config ✅ created |
| `src/robot_bringup/launch/ekf.launch.xml` | Launches robot_localization node ✅ created |
| `src/robot_bringup/launch/lidar.launch.xml` | Launches RPLidar A1 node ✅ created |
| `src/robot_bringup/launch/realsense.launch.xml` | Launches RealSense driver ✅ already existed |

### Step-by-step

**Step 1 — micro-ROS agent**
The micro-ROS agent runs as `microros-agent.service` on the Pi — no separate launch needed. It uses native USB CDC (`/dev/ttyACM0`) at 921600 baud. Confirm it is active:
```bash
systemctl is-active microros-agent.service
ros2 topic hz /diff_cont/odom   # must show ~30 Hz
```

**Step 2 — LiDAR driver**
Launch via `lidar.launch.xml`. Frame ID must be `laser`. Expected rate: ~5.5 Hz on `/scan`.

```bash
ros2 launch robot_bringup lidar.launch.xml
```

**LiDAR must pass independently before continuing:**
```bash
ros2 topic hz /scan                       # must be ~5.5 Hz
ros2 topic echo /scan --once | head -10   # ranges must not be all-zero or all-inf
```
Do not proceed to Step 3 until these pass.

**Step 3 — RealSense driver**
Launch via `realsense.launch.xml` (config: 424×240 depth @ 6 Hz, color @ 15 Hz, RGB8 point cloud):
```bash
ros2 launch robot_bringup realsense.launch.xml
```
See [`docs/testing/phase3_realsense_validation_2026-05-24.md`](../testing/phase3_realsense_validation_2026-05-24.md) for validated config and known v4.x issues.

**Step 4 — EKF**
`ekf.yaml` fuses:
- `/diff_cont/odom` — x, y, yaw position + vx, vyaw velocity
- `/imu/imu` — angular velocity z + linear acceleration x/y

IMU orientation **disabled** (BNO055 magnetometer unreliable on metal chassis). `two_d_mode: true`. Base frame: `base_footprint`. Output: `/odom` at 20 Hz, `odom → base_footprint` TF.

```bash
ros2 launch robot_bringup ekf.launch.xml
```

**Step 5 — PID floor tuning**
Before declaring Phase 3 complete, validate that wheel velocity tracks commands within ±10% at steady state on the floor. Use `ros2 bag record` to capture odom + IMU during a straight-line drive. Adjust `PID_KI` in `firmware/esp32/include/pid.h` if tracking is slow.

### Validation gate — Phase 3

```bash
ros2 topic hz /scan                              # ~5.5 Hz
ros2 topic hz /camera/camera/depth/color/points  # ~6 Hz
ros2 topic hz /diff_cont/odom                    # ~30 Hz
ros2 topic hz /odom                              # ~20 Hz — EKF output
ros2 run tf2_ros tf2_echo odom base_footprint    # must update smoothly while robot moves
```

Drive the robot slowly in a straight line. `/odom` pose should track accurately with no jumps.

Phase 3 is complete when all topic rates are met, `/odom` tracks motion smoothly, and PID velocity error is ≤ ±10% at steady state.

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

**Step 1 — slam_toolbox config**
Key params in `slam_toolbox.yaml`:
- `odom_frame: odom`
- `map_frame: map`
- `base_frame: base_link`
- `scan_topic: /scan`
- `mode: mapping`
- Tune `resolution`, `max_laser_range`, and loop closure params for indoor use.

**Step 2 — Mapping launch**
`slam.launch.py` launches `async_slam_toolbox_node` with the config. It should also bring up `description.launch.py` and `ekf.launch.py` as dependencies (or use `robot_bringup`).

**Step 3 — Map save / load**
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

**Step 1 — Nav2 params**
Configure `nav2_params.yaml` with:
- Controller: `DWBLocalPlanner` or `RPP` (regulated pure pursuit)
- Costmap layers: `static_layer` (SLAM map) → `obstacle_layer` (LiDAR `/scan`) → `voxel_layer` (RealSense, placeholder for Phase 6)
- Global/local costmap inflation radius tuned for robot footprint
- Controller frequency: 20 Hz
- Command topic: `/diff_cont/cmd_vel_unstamped`

**Step 2 — Navigation launch**
`navigation.launch.py` launches:
- `nav2_bringup` with `nav2_params.yaml`
- Map server with saved map from Phase 4
- AMCL for localization (or slam_toolbox localization mode)

**Step 3 — Top-level bringup**
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
BME680 environmental data streaming, RealSense depth integrated into Nav2 as a voxel costmap layer, and OLED status display running on the Pi.

### Files to create / modify

| File | Purpose |
|---|---|
| `firmware/esp32/src/env_sensor.cpp` | BME680 I2C read + publish |
| `firmware/esp32/include/env_sensor.h` | BME680 interface |
| `src/robot_msgs/msg/EnvData.msg` | Custom message for BME680 data |
| `src/robot_bringup/config/nav2_params.yaml` | Enable voxel_layer with RealSense |
| `scripts/display_daemon.py` | Pi OLED display daemon — reads `/battery_state` via rclpy, psutil for system stats |
| `scripts/mybot-display.service` | systemd unit for display daemon |
| `scripts/microros-agent.service` | systemd unit for micro-ROS agent (persistent, auto-restart) |

### Step-by-step

**Step 1 — BME680 firmware**
Wire BME680 to I2C bus (addr 0x76). Read temperature, humidity, pressure, gas resistance. Publish as custom `robot_msgs/EnvData` or `sensor_msgs/Temperature` + `sensor_msgs/RelativeHumidity` at 1 Hz. Add to micro-ROS node.

Library: https://github.com/adafruit/Adafruit_BME680 — add `adafruit/Adafruit BME680 Library` to `platformio.ini`. Confirm no address conflict with BNO055 (0x28) and INA219 (0x40) before wiring.

**FreeRTOS isolation required:** The BME680 reads at 1 Hz on the shared I2C bus. Run it in the same `battery_task` (core 0) or a sibling task pinned to core 0 — never in `loop()` or the PID task. The same isolation rule that applies to INA219 applies here.

**Step 2 — RealSense voxel layer**
In `nav2_params.yaml`, enable `voxel_layer` in both global and local costmaps. Subscribe to `/camera/depth/points`. Tune height range to detect obstacles between 0.05 m and 1.5 m above floor.

**Step 3 — OLED display daemon**
Wire OLED to Pi SPI0 per wiring table in `docs/hardware/oled_display.md`. Enable SPI via `raspi-config`.

The display is driven by a systemd daemon (`scripts/display_daemon.py`), not a ROS node. It reads battery data by spawning a `ros2 topic echo /battery_state` subprocess and parsing its output — this avoids rclpy threading / DDS initialization issues in a long-running service. It reads system stats via `psutil`. If ROS2 is down it falls back gracefully with `ROS:--`.

**Dependency:** `microros-agent.service` must also be deployed so the agent auto-restarts and provides the `/battery_state` feed on boot.

Display layout (128×64, 5 rows at 9pt monospace):
| Row | Content | Source |
|---|---|---|
| 1 | Hostname + IP address | socket / psutil |
| 2 | Battery bar (visual fill) + % | `/battery_state` subprocess |
| 3 | Voltage + current + Pi CPU temperature | `/battery_state` + psutil |
| 4 | CPU % + RAM % | psutil |
| 5 | `ROS:OK`/`ROS:--` + uptime | subprocess freshness |

Files to deploy:
- `scripts/display_daemon.py` — subprocess battery reader + psutil, renders via luma.oled over SPI at 2 Hz
- `scripts/mybot-display.service` — `User=ubuntu`, `WorkingDirectory=/tmp`, sources ROS2 setup
- `scripts/microros-agent.service` — `Restart=always`, `StartLimitIntervalSec=0`, `RestartSec=1`

**Step 3a — Pi INA219 (battery monitor independent of ESP32)**
Wire a second INA219 directly to the Pi's I2C-1 bus (GPIO 2/3) so battery voltage is always available, even before micro-ROS establishes its session (which can take 1–6 minutes after boot). See [`docs/hardware/ina219_pi_battery_monitor.md`](../hardware/ina219_pi_battery_monitor.md) for full wiring and software integration.

When wired: update `display_daemon.py` `BatteryReader` to use `smbus2`/`pi-ina219` directly instead of the subprocess. Battery will appear on the OLED from the first second of boot.

Required hardware: 1× Adafruit INA219 breakout, tap from battery+ rail (after 3A fuse), Pi header pins 1/3/5/6.

Install and enable:
```bash
sudo cp scripts/mybot-display.service /etc/systemd/system/
sudo cp scripts/microros-agent.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mybot-display.service microros-agent.service
sudo systemctl start mybot-display.service microros-agent.service
```

> **Pi 5 lgpio note:** The lgd daemon creates pipe files in the working directory. `WorkingDirectory=/tmp` is required. Both services must run as `ubuntu` — FastDDS shared memory is user-scoped and the display daemon must share a user with the micro-ROS agent.

Hardware reference: [`docs/hardware/oled_display.md`](../hardware/oled_display.md)

### Validation gate — Phase 6

```bash
ros2 topic echo /env_data --once    # must show plausible temp/humidity/pressure
ros2 topic hz /camera/depth/points  # still ~15 Hz
# In RViz2: 3D obstacle visible in local costmap when object held in front of camera
sudo systemctl status mybot-display.service   # must be active/running
sudo systemctl status microros-agent.service  # must be active/running
# OLED shows: hostname+IP, battery bar + %, voltage/current/temp, CPU/RAM, ROS:OK
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

**Step 1 — YOLO node**
Subscribe to `/camera/color/image_raw`. Run YOLOv8 inference. Publish detections as `vision_msgs/Detection2DArray` on `/detections`. Target: ≥10 FPS on dev PC GPU.

**Step 2 — Semantic costmap layer**
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
