# INA219 Battery Monitor — Raspberry Pi (Direct)

Second INA219 wired directly to the Pi for battery monitoring independent of ESP32 / micro-ROS.

**Why:** Display and health check need battery voltage at boot, before micro-ROS establishes its session (which can take 1–6 minutes). Reading from the Pi directly via smbus2 eliminates the ROS2 dependency for battery display.

**Note:** The existing INA219 on the ESP32 I2C bus (0x28/0x40) remains unchanged — the ESP32 uses it for battery cutoff (safety watchdog). This is a second, independent chip for monitoring only.

---

## Hardware

| Item | Value |
|---|---|
| Chip | Adafruit INA219 breakout (same as ESP32 unit) |
| I2C address | 0x40 (A0=GND, A1=GND — same default address, different bus) |
| Pi I2C bus | I2C-1 (GPIO 2 SDA, GPIO 3 SCL — 40-pin header) |
| Shunt | On-board 0.1 Ω (100 mA rated — sufficient for monitoring; ESP32 INA219 handles cutoff) |
| Max bus voltage | 26V (INA219 spec — safe for 3S LiPo ~12.6V) |
| Logic voltage | 3.3V from Pi 3V3 pin |

---

## Wiring Diagram

```
Battery positive rail (after main 3A fuse, before EP-0225 barrel jack)
        │
        ├──────────────────────────────── INA219 VIN+
        │                                      │
        │                               [0.1Ω shunt]
        │                                      │
        │                                 INA219 VIN−  ──── (load not needed for voltage-only monitoring)
        │
        │   INA219 breakout
        │   ┌────────────┐
        └───┤ VIN+       │
            │ VIN−       │  (shunt in series; leave VIN− open for voltage-only read)
            │            │
            │ VCC ───────┼──── Pi 3V3  (header pin 1 or 17)
            │ GND ───────┼──── Pi GND  (header pin 6, 9, 14, 20, 25, 30, 34, or 39)
            │ SDA ───────┼──── Pi GPIO 2  (header pin 3)  ← I2C-1 SDA
            │ SCL ───────┼──── Pi GPIO 3  (header pin 5)  ← I2C-1 SCL
            │ A0  ───────┼──── GND  (address bit 0 = 0)
            │ A1  ───────┼──── GND  (address bit 1 = 0) → I2C address 0x40
            └────────────┘
```

### Pi 40-pin header reference (relevant pins)

```
 3V3  [1] [2]  5V
 SDA  [3] [4]  5V
 SCL  [5] [6]  GND
```

Connect INA219 to pins 1 (3V3), 3 (SDA), 5 (SCL), 6 (GND).

### Battery tap point

Tap VIN+ from the **battery positive rail after the 3A fuse** — the same rail that feeds EP-0225. Do NOT tap from the 5V USB output of EP-0225 (that's regulated, not battery voltage).

```
Battery+  →  [3A fuse]  →  ┬──  EP-0225 barrel jack IN
                            └──  INA219 VIN+   (new tap here)
```

---

## Enabling I2C on Pi

```bash
# Verify I2C is enabled (should already be on for most Ubuntu Pi builds)
ls /dev/i2c-*          # expect /dev/i2c-1

# Scan for INA219
sudo i2cdetect -y 1    # expect 0x40

# If i2cdetect not installed:
sudo apt install i2c-tools
```

If `/dev/i2c-1` doesn't exist, enable I2C:
```bash
sudo raspi-config       # Interface Options → I2C → Enable
# OR edit /boot/firmware/config.txt:
echo "dtparam=i2c_arm=on" | sudo tee -a /boot/firmware/config.txt
sudo reboot
```

---

## Software — smbus2 read

Install driver:
```bash
pip3 install smbus2 pi-ina219
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

Or raw smbus2:
```python
import smbus2, struct

bus = smbus2.SMBus(1)
INA219_ADDR = 0x40
raw = bus.read_i2c_block_data(INA219_ADDR, 0x02, 2)   # bus voltage register
val = struct.unpack('>H', bytes(raw))[0]
voltage_V = (val >> 3) * 0.004   # 4 mV per LSB
```

---

## display_daemon.py integration

When the Pi INA219 is wired, update `display_daemon.py` to read from smbus2 directly instead of the `ros2 topic echo` subprocess:

```python
from ina219 import INA219

class BatteryReader:
    def __init__(self):
        self._ina = INA219(0.1, busnum=1)
        self._ina.configure()
        self.voltage = 0.0
        self.current = 0.0
        self._last_read = 0.0
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        while True:
            try:
                self.voltage = self._ina.voltage()
                self.current = self._ina.current() / 1000.0   # mA → A
                self._last_read = time.monotonic()
            except Exception as e:
                print(f"INA219 read error: {e}", flush=True)
            time.sleep(1.0)

    @property
    def fresh(self):
        return self._last_read > 0 and (time.monotonic() - self._last_read < 3.0)
```

Remove the `ros2 topic echo` subprocess entirely. Battery will be available from the first second of boot, regardless of ESP32 or micro-ROS state.

---

## Two-INA219 summary

| Unit | Bus | Address | Reader | Purpose |
|---|---|---|---|---|
| ESP32 INA219 | ESP32 I2C (GPIO 8/9) | 0x40 | ESP32 firmware | Battery cutoff safety watchdog, publishes `/battery_state` |
| Pi INA219 | Pi I2C-1 (GPIO 2/3) | 0x40 | smbus2 in display_daemon | Display + health check, always-on regardless of ROS2 |
