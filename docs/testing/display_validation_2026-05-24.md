# OLED Display Validation — 2026-05-24

## Goal

Wire and validate the Waveshare 2.42" SSD1309 OLED display on the Raspberry Pi 5.
Confirm the display daemon renders system stats without ROS2 running.

---

## Hardware

- Waveshare 2.42inch OLED Module (SSD1309, 128×64, 4-wire SPI)
- Raspberry Pi 5, Ubuntu Server 24.04 LTS

## Wiring (SPI0, BCM pin numbers)

| OLED Pin | Pi BCM GPIO | Pi Physical Pin |
|---|---|---|
| VCC | 3.3V | Pin 1 |
| GND | GND | Pin 6 |
| DIN | GPIO 10 (MOSI) | Pin 19 |
| CLK | GPIO 11 (SCLK) | Pin 23 |
| CS | GPIO 8 (CE0) | Pin 24 |
| DC | GPIO 25 | Pin 22 |
| RES | GPIO 27 | Pin 13 |

---

## Software Setup (Pi, one-time)

```bash
# SPI already enabled (confirmed: /dev/spidev0.0 present at boot)

# Install Python deps system-wide (required for root/systemd)
sudo pip3 install --break-system-packages luma.oled lgpio spidev pyserial

# Install and enable systemd service
sudo cp ~/bot_ws/scripts/mybot-display.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable mybot-display
sudo systemctl start mybot-display
```

---

## Issues Encountered

### Issue 1 — `No module found: RPi`

luma.core 2.5.3 defaults to RPi.GPIO for GPIO control. RPi.GPIO is unavailable on Pi 5/Ubuntu 24.04.

**Fix:** Added `LGPIOAdapter` class to `display_daemon.py` — wraps `lgpio` (gpiochip4, the Pi 5 main GPIO chip) with the minimal RPi.GPIO interface that luma.core expects (`setmode`, `setup`, `output`, `cleanup`). Passed explicitly to the `spi()` constructor:

```python
serial_obj = spi(..., gpio=LGPIOAdapter())
```

### Issue 2 — `No module found: spidev`

`spidev` must be installed separately from `luma.oled`.

**Fix:** `sudo pip3 install --break-system-packages spidev`

### Issue 3 — luma installed for user, not root

Initial `pip3 install` installed luma for the `ubuntu` user. Running with `sudo` used root's Python env where luma wasn't present.

**Fix:** `sudo pip3 install --break-system-packages luma.oled lgpio spidev`

---

## Result

- Display renders on hardware ✓
- Shows: IP address, CPU%, temperature, RAM%, ESP32 status placeholder ✓
- systemd service installed and enabled (auto-starts at boot) ✓
- ROS2-independent — works without micro-ROS agent or any ROS2 nodes ✓

---

## Daemon Source

`scripts/display_daemon.py` — renders 2 Hz updates over SPI0.
`scripts/mybot-display.service` — systemd unit, runs as `ubuntu` user.

---

## Architecture Update — 2026-05-25

Battery telemetry architecture changed from CH340 UART JSON (`/dev/ttyUSB0`) to ROS2 `/battery_state` topic (rclpy subscriber in a background thread). The daemon now sources battery data from micro-ROS via the standard `/battery_state` topic rather than a bespoke serial stream. Falls back gracefully to `no ROS2` display when the topic is stale.

A second service, `scripts/microros-agent.service`, was added to keep the micro-ROS agent alive persistently under systemd (`Restart=always`, `StartLimitIntervalSec=0`).

Display layout also updated to 5 rows: hostname+IP, visual battery bar + %, voltage/current/Pi temperature, CPU/RAM, ROS status + uptime. Battery showing live on hardware: 11.8V 0.43A 69%.

**Root cause of original "no ROS2" symptom:** Two micro-ROS agents contending on `/dev/ttyACM0` (orphaned SSH-session agent + systemd service). Resolved by always running the agent exclusively via `microros-agent.service`.

**lgpio user-scope issue:** FastDDS shared memory is user-scoped; daemon must run as the same user as the micro-ROS agent (`ubuntu`). Root-owned lgd daemon also caused lgpio notification failures — fixed by ensuring `WorkingDirectory=/tmp` and running as `ubuntu`.
