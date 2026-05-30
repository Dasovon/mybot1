# INA219 Battery Monitor — Raspberry Pi (Direct)

Second INA219 wired directly to the Pi for battery monitoring independent of ESP32 / micro-ROS.

**Why:** Display and health check need battery voltage at boot, before micro-ROS establishes its session (which can take 1–6 minutes). Reading from the Pi directly via smbus2 eliminates the ROS2 dependency for battery display.

**Note:** The existing INA219 on the ESP32 I2C bus (GPIO 8/9, 0x40) remains unchanged — the ESP32 uses it for battery cutoff (safety watchdog). This is a second, independent chip for monitoring only.

---

## Hardware

| Item | Value |
|---|---|
| Chip | Generic INA219 DC current sensor module (6-pin header + screw terminals) |
| I2C address | 0x40 (A0=GND, A1=GND — default, address pads unsoldered) |
| Pi I2C bus | I2C-1 (GPIO 2 SDA, GPIO 3 SCL — 40-pin header) |
| Shunt | On-board 0.1 Ω (R100) |
| Max bus voltage | 26V (INA219 spec — safe for 3S LiPo ~12.6V) |
| Logic voltage | 3.3V from Pi Pin 17 |

---

## Board Pinout

This module has two connectors:

**Left side — screw terminals (power measurement, in-series with load):**
```
1  Vin+   Battery positive input (before shunt)
2  Vin−   Load output (after shunt)
```

**Right side — 6-pin header (I2C + power):**
```
1  Vin+   (same node as left screw terminal Vin+)
2  Vin−   (same node as left screw terminal Vin−)
3  SDA    I2C data
4  SCL    I2C clock
5  GND    Ground
6  VCC    Logic supply (3.3V from Pi)
```

---

## Wiring

### Power path — INA219 in series on the logic rail

The INA219 shunt is inserted between the 3A fuse and the EP-0225 barrel jack input.
This measures all current drawn by the Pi + ESP32 logic rail.

```
Battery+  →  [3A fuse]  →  INA219 Vin+  →  [0.1Ω shunt]  →  INA219 Vin−  →  EP-0225 barrel jack IN
```

Use the **screw terminals** on the left side of the board for Vin+ and Vin−.

### Pi header connections — 6-pin header on right side

```
Board pin 3 (SDA)  →  Pi Pin 3   (GPIO 2, I2C-1 SDA)
Board pin 4 (SCL)  →  Pi Pin 5   (GPIO 3, I2C-1 SCL)
Board pin 5 (GND)  →  Pi Pin 6   (GND)
Board pin 6 (VCC)  →  Pi Pin 17  (3.3V)  ← use Pin 17; Pin 1 is used by display
```

### Pi 40-pin header reference (relevant pins)

```
 3V3  [1]  [2]  5V
 SDA  [3]  [4]  5V
 SCL  [5]  [6]  GND
      ...
 3V3 [17] [18]
```

### Common ground

Battery −, Pi GND, and INA219 GND share the same rail through the EP-0225 barrel jack.
No extra ground wire needed.

---

## Current measurement note

The on-board shunt is 0.1 Ω. The Pi 5 can draw up to 5A at full load:

- 5A × 0.1Ω = 500mV shunt drop — exceeds the INA219's ±320mV shunt amplifier range
- Current readings will saturate above ~3.2A
- **Bus voltage readings are always accurate regardless of current**

For this robot, peak logic rail current is ~1–2A (Pi idle + ESP32), well within range.

---

## Enabling I2C on Pi

```bash
# Verify I2C is enabled
ls /dev/i2c-*          # expect /dev/i2c-1

# Scan for INA219
sudo i2cdetect -y 1    # expect 0x40

# If i2cdetect not installed:
sudo apt install i2c-tools

# Add ubuntu user to i2c group (avoids sudo for every read):
sudo usermod -aG i2c ubuntu
sudo chmod a+rw /dev/i2c-1   # immediate effect; group takes effect on next login
```

If `/dev/i2c-1` doesn't exist:
```bash
echo "dtparam=i2c_arm=on" | sudo tee -a /boot/firmware/config.txt
sudo reboot
```

---

## Software — pi-ina219

```bash
pip3 install pi-ina219 --break-system-packages
```

Quick test:
```python
from ina219 import INA219

ina = INA219(0.1, busnum=1)   # 0.1Ω shunt, I2C bus 1
ina.configure()
print(f"Voltage: {ina.voltage():.2f} V")
print(f"Current: {ina.current():.1f} mA")
print(f"Power:   {ina.power():.1f} mW")
```

---

## display_daemon.py integration

`scripts/display_daemon.py` reads from the INA219 directly via pi-ina219.
The `BatteryReader` class polls at 1 Hz in a background thread.
No ROS2 subprocess — battery is available from the first second of boot.

---

## Two-INA219 summary

| Unit | Bus | Address | Reader | Purpose |
|---|---|---|---|---|
| ESP32 INA219 | ESP32 I2C (GPIO 8/9) | 0x40 | ESP32 firmware | Battery cutoff safety watchdog, publishes `/battery_state` |
| Pi INA219 | Pi I2C-1 (GPIO 2/3) | 0x40 | pi-ina219 in display_daemon | Display + health check, always-on regardless of ROS2 state |
