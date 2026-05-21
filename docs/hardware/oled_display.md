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
pip3 install lgpio luma.oled psutil
```

> **lgpio is required on Ubuntu 22.04 / Bookworm.** bcm2835 and WiringPi are deprecated and do not work on Raspberry Pi 5.
>
> **luma.oled** is the display rendering library. **psutil** provides CPU/RAM/temperature data.

Waveshare library GitHub: https://github.com/waveshare/2.42inch-OLED-Module

### Step 3 — Download Waveshare demo (hardware verification only)

```bash
git clone https://github.com/waveshare/2.42inch-OLED-Module.git ~/oled_demo
cd ~/oled_demo/RaspberryPi/python/
sudo python3 OLED_2in42_test.py
```

The display should cycle through: text, rectangles, ellipses, and a logo. This confirms the SPI wiring is correct before deploying the daemon.

---

## Display Daemon

The robot uses a **ROS2-independent display daemon**, not a ROS node. This is the key design choice: the OLED works at boot before ROS2 starts, during ROS2 crashes, and during ESP32 reflashing.

### How it works

```
ESP32 Serial0 (USB CDC, /dev/ttyACM0)
    → JSON stream: {"v":12.34,"i":1.23,"p":15.16,"ok":1,"ts":12345}
        → display_daemon.py reads + parses
            → renders 128×64 frame via luma.oled over SPI
                → pushes to SSD1309 at 2 Hz
```

System stats (CPU, RAM, IP) come from `psutil` — no ROS2 dependency.

### Display layout

| Line | Content | Source |
|---|---|---|
| 1 | IP address | psutil / socket |
| 2 | Battery voltage + current | ESP32 Serial0 JSON |
| 3 | Topic rates: `/diff_cont/odom`, `/scan` | psutil process check or ROS2 optional |
| 4 | CPU % + RAM % | psutil |

### Files

| File | Location | Purpose |
|---|---|---|
| `display_daemon.py` | `scripts/` | Main daemon — serial read, render, SPI push |
| `mybot-display.service` | `scripts/` | systemd unit file |

### systemd service

```ini
# scripts/mybot-display.service
[Unit]
Description=MyBot OLED Display Daemon
After=multi-user.target

[Service]
Type=simple
User=ryan
Group=dialout
ExecStart=/usr/bin/python3 /home/ryan/bot_ws/scripts/display_daemon.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Install and enable:
```bash
sudo cp scripts/mybot-display.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mybot-display.service
sudo systemctl start mybot-display.service
sudo systemctl status mybot-display.service   # should show active (running)
```

### Serial0 JSON format

The ESP32 streams compact JSON over Serial0 at 2 Hz:
```json
{"v":12.34,"i":1.23,"p":15.16,"ok":1,"ts":12345}
```

| Key | Meaning |
|---|---|
| `v` | Bus voltage (V) |
| `i` | Current (A) |
| `p` | Power (W) |
| `ok` | 1 = sensors healthy, 0 = I2C error |
| `ts` | Timestamp (ms since boot) |

> The ESP32 must have Serial0 publishing this format. This is implemented in Phase 6 firmware alongside the BME680 addition. Serial0 is separate from Serial1 (micro-ROS) — they run independently.

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
