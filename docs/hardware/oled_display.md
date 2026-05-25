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

The display daemon is a systemd service (`mybot-display.service`) running as `ubuntu` on the Pi. It is not a ROS node, but it does subscribe to the `/battery_state` ROS2 topic via rclpy to get live battery data. If ROS2 or the micro-ROS agent is not running, the daemon continues to display system stats and shows `no ROS2` in the battery row.

### How it works

```
micro-ROS agent (microros-agent.service)
    → /battery_state topic (sensor_msgs/BatteryState) at 1 Hz
        → display_daemon.py BatteryReader (rclpy subscriber, background thread)
            → renders 128×64 frame via luma.oled over SPI at 2 Hz
                → pushes to SSD1309
System stats (hostname, IP, CPU, RAM, temperature) come from psutil — always available.
```

### Display layout (128×64, 5 rows)

| Row | Content | Source |
|---|---|---|
| 1 | Hostname + IP address | socket / psutil |
| 2 | Battery bar (visual, fills left-to-right) + % | `/battery_state` |
| 3 | Voltage + current + Pi CPU temperature | `/battery_state` + psutil |
| 4 | CPU % + RAM % | psutil |
| 5 | ROS status (`ROS:OK` / `ROS:--`) + uptime | rclpy freshness check |

When `/battery_state` data is fresh (< 3 s old): row 2 bar fills, row 3 shows live numbers, row 5 shows `ROS:OK`.
When not fresh: bar is empty, row 3 shows `no ROS2  T:52C`, row 5 shows `ROS:--`.

### Files

| File | Location | Purpose |
|---|---|---|
| `display_daemon.py` | `scripts/` | Main daemon — rclpy subscriber, psutil stats, SPI render |
| `mybot-display.service` | `scripts/` | systemd unit for display daemon |
| `microros-agent.service` | `scripts/` | systemd unit for micro-ROS agent (battery data source) |

### systemd service

```ini
# scripts/mybot-display.service
[Unit]
Description=mybot1 OLED Display Daemon
After=multi-user.target

[Service]
Type=simple
User=ubuntu
WorkingDirectory=/tmp
ExecStart=/bin/bash -c "source /opt/ros/jazzy/setup.bash && exec python3 /home/ubuntu/bot_ws/scripts/display_daemon.py"
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

`WorkingDirectory=/tmp` is required — lgpio creates notification pipes in the working directory.
`User=ubuntu` is required — FastDDS shared memory is user-scoped; the display daemon must run as the same user as the micro-ROS agent.

Install and enable both services:
```bash
sudo cp scripts/mybot-display.service /etc/systemd/system/
sudo cp scripts/microros-agent.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mybot-display.service microros-agent.service
sudo systemctl start mybot-display.service microros-agent.service
sudo systemctl status mybot-display.service microros-agent.service
```

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
