# Waveshare 2.42inch OLED Module — Status Display

## Role in This Project

The OLED display shows robot status information on the physical chassis — IP address, ROS topic health, battery voltage, and CPU load. It connects to the Raspberry Pi via SPI0 and is driven by a small Python script. It is not part of the ROS control loop.

---

## Key Specs

| Property | Value |
|---|---|
| Model | Waveshare 2.42inch OLED Module |
| Driver IC | SSD1309 |
| Resolution | 128 × 64 pixels |
| Color | White (part 19613) or Yellow (part 19614) — monochrome |
| Interface (default) | 4-wire SPI (factory default) |
| Interface (optional) | I2C — switchable via solder resistors on back |
| Supply voltage | 3.3V or 5V (onboard 3.3V regulator) |
| Module size | 61.50 × 39.50 mm |
| Display area | 55.01 × 27.49 mm |
| I2C addresses | DC=LOW → 0x3C, DC=HIGH → 0x3D |

---

## Pin Functions

**Interface used in this build: 4-wire SPI (factory default).** Connects to **Raspberry Pi 5**, not the ESP32.

| OLED Pin | SPI mode | I2C mode |
|---|---|---|
| VCC | Power 3.3V or 5V | Power 3.3V or 5V |
| GND | Ground | Ground |
| DIN | MOSI (data in) | SDA |
| CLK | SCLK | SCL |
| CS | SPI chip select (active low) | NC (tie to GND) |
| DC | Data/cmd select (HIGH=data, LOW=cmd) | Address select (LOW=0x3C, HIGH=0x3D) |
| RES | Reset (active low) | Reset (active low) |

> **DC pin warning:** DC and MISO are on adjacent pins in the same header row. Wrong pin = blank display, no error.

---

## Software Setup

**All commands run on the Raspberry Pi.**

### Step 1 — Enable SPI

```bash
sudo raspi-config
# → Interface Options → SPI → Yes → Finish → reboot
```

Verify SPI kernel module loaded after reboot:
```bash
ls /dev/spidev*   # expect /dev/spidev0.0
```

### Step 2 — Install dependencies

```bash
sudo apt update
sudo apt install python3-pip python3-pil python3-spidev python3-smbus -y
pip3 install lgpio
```

> **lgpio is required on Ubuntu 22.04 / Bookworm.** bcm2835 and WiringPi are deprecated and do not work on Raspberry Pi 5.

Waveshare library GitHub: https://github.com/waveshare/2.42inch-OLED-Module

### Step 3 — Download Waveshare demo

```bash
git clone https://github.com/waveshare/2.42inch-OLED-Module.git ~/oled_demo
cd ~/oled_demo/RaspberryPi/python/
```

### Step 4 — Run test

```bash
sudo python3 OLED_2in42_test.py
```

The display should cycle through: text, rectangles, ellipses, and a logo.

---

## Status Display Script

For this robot, the display script will be in `scripts/oled_status.py`. It runs as a background service on the Pi and shows:

| Line | Content |
|---|---|
| 1 | IP address |
| 2 | Battery voltage (from `/battery_state`) |
| 3 | Topic rates: `/odom`, `/scan` |
| 4 | CPU load % |

The script uses `Pillow` (PIL) to compose the frame and `spidev` to push pixels to the SSD1309.

---

## Common Failures

| Symptom | Likely Cause |
|---|---|
| Display blank, no error | DC or RES pin wired to wrong header pin — double-check pin numbers |
| `FileNotFoundError: /dev/spidev0.0` | SPI not enabled — run `raspi-config` and reboot |
| `ImportError: lgpio` | lgpio not installed — `pip3 install lgpio` |
| ImportError for bcm2835 | bcm2835 deprecated on Pi 5 — switch to lgpio |
| Partial or garbled image | VCC is 5V but display pin not tolerant — switch to 3.3V VCC |
| Display works standalone, fails in ROS session | sudo required for GPIO access — run script as root or configure udev rule |
