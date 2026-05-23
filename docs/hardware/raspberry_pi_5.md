# Raspberry Pi 5 — Sensor Bridge / ROS Interface

## Role in This Project

The Raspberry Pi 5 is the onboard compute node. It acts as the sensor bridge between raw hardware (ESP32, LiDAR, RealSense) and the ROS 2 graph. It runs lightweight ROS 2 nodes and forwards data to the development PC over Wi-Fi or Ethernet.

---

## Responsibilities

| Domain | Details |
|---|---|
| Serial bridge | Reads ESP32 telemetry, sends velocity commands |
| LiDAR driver | Hosts RPLIDAR ROS 2 driver, publishes `/scan` |
| RealSense driver | Hosts RealSense ROS 2 wrapper, publishes depth/RGB |
| ROS 2 node host | Runs lightweight nodes (no SLAM/Nav2 on Pi) |
| Network bridge | Connects to dev PC over Wi-Fi or Ethernet |
| Local safety | Can issue STOP to ESP32 if local fault detected |

---

## Key Specs (Raspberry Pi 5)

| Property | Value |
|---|---|
| CPU | Broadcom BCM2712, Cortex-A76 quad-core @ 2.4 GHz |
| RAM | 4 GB or 8 GB LPDDR4X |
| Storage | microSD or NVMe SSD via PCIe (recommended: NVMe) |
| USB | 2× USB 3.0, 2× USB 2.0 |
| Networking | Gigabit Ethernet, Wi-Fi 802.11ac (5 GHz), Bluetooth 5.0 |
| GPIO | 40-pin header (standard RPi pinout) |
| Power | 5V / 5A via USB-C PD |
| OS | Ubuntu Server 24.04 LTS (64-bit) |

---

## USB Device Assignments

| Device | Pi USB Port | Device Path | Notes |
|---|---|---|---|
| ESP32-S3 | USB-A (any) | `/dev/serial/by-id/usb-Espressif_USB_JTAG_...` | micro-ROS serial transport |
| RPLidar A1 | USB-A (USB 2.0 OK) | `/dev/rplidar` (udev symlink) | CP2102, VID:10c4 PID:ea60 |
| RealSense D435 | USB-A (**USB 3.0 — blue**) | Managed by librealsense | **Must be USB 3.0** — USB 2.0 bandwidth insufficient |

Power is provided by the **EP-0225 (52pi)** via USB PD 3.0 (5.15V/5A) to the Pi 5 USB-C power port. See [rpi5_pd_power_hat.md](rpi5_pd_power_hat.md).

---

## ROS 2 Nodes Running on Pi

| Node | Package | Topic(s) |
|---|---|---|
| `esp32_serial_bridge` | `esp32_serial_bridge` | `/diff_cont/odom`, `/imu/imu`, `/battery_state`, `/diff_cont/cmd_vel_unstamped` |
| `rplidar_node` | `rplidar_ros` | `/scan` |
| `realsense2_camera_node` | `realsense2_camera` | `/camera/depth/*`, `/camera/color/*` |

SLAM, Nav2, and AI nodes run on the development PC, not the Pi.

---

## Networking

| Mode | Interface | Notes |
|---|---|---|
| Development | Ethernet (preferred) | Lower latency, more stable for ROS DDS |
| Mobile | Wi-Fi 5 GHz | Use 5 GHz for reduced interference |

Set `ROS_DOMAIN_ID` consistently across Pi and dev PC.
Configure `cyclonedds` or `fastdds` with the correct network interface.

---

## Power

- Power input: USB-C PD, 5V/5A (25W recommended)
- The Pi 5 draws up to 25W under heavy load — use a dedicated regulator
- Use a dedicated 5V/5A regulator from the robot battery (not shared with motors)
- A Pi 5 active cooler is strongly recommended

---

## Common Ground

The Pi's GND must be tied to the ESP32 GND and battery negative.
Do not float the Pi ground — this causes serial communication errors.

---

## Storage Recommendation

Use an NVMe SSD via the Pi 5 PCIe M.2 HAT for rosbag recording. microSD cards are too slow for continuous bag recording at high data rates (LiDAR + RealSense).

---

## Stable Baseline (as of Phase 0)

| Item | Value |
|---|---|
| OS | Ubuntu Server 24.04 LTS (64-bit) |
| Hostname | `pi5bot` |
| Username | `ubuntu` |
| SSH | `ssh ubuntu@pi5bot.local` or `ssh ubuntu@<PI_IP>` |

**Key lesson from bring-up:** Do NOT use Imager customization (username, hostname, SSH, WiFi) when flashing Ubuntu Server — this triggers the cloud-init provisioning path which fails intermittently on Pi 5. Flash with no customization, log in as the default `ubuntu` user, then configure manually.

### ROS 2 Jazzy on Ubuntu 24.04

Ubuntu 24.04 is Tier 1 for ROS 2 Jazzy — standard apt packages, no workarounds. The dev PC must also run Ubuntu 24.04 + Jazzy for cross-machine topic communication.

## Setup Notes

### Imager bring-up procedure

```
OS:      Ubuntu Server 24.04 LTS (64-bit)
Device:  Raspberry Pi 5
Storage: microSD (≥32 GB)
Customization: NONE — do not set hostname/user/SSH/WiFi in Imager
```

First boot — Pi connected via Ethernet only, no other USB hardware:

```bash
# Find Pi IP (check router/Google Wifi app, or:)
nmap -sn 192.168.86.0/24 | grep -i ubuntu

# Clear stale host key if reflashed:
ssh-keygen -f "/home/ryan/.ssh/known_hosts" -R "<PI_IP>"

# SSH in — default credentials:
ssh ubuntu@<PI_IP>
# password: ubuntu  (forced to change on first login)
```

After password change, set hostname:
```bash
sudo hostnamectl set-hostname pi5bot
sudo sed -i 's/127.0.1.1.*/127.0.1.1 pi5bot/' /etc/hosts
```

Configure WiFi (optional — keep Ethernet for ROS traffic):
```bash
sudo nmtui
```

### USB power — required config.txt entry
The Pi 5 limits USB port power to 600mA if it doesn't successfully negotiate 5V/5A with the power supply. The EP-0225 (52pi) may not advertise 5V/5A in a way the Pi recognizes. Without this flag, RPLidar (~500mA peak) and RealSense (~900mA) are at risk of undervoltage.

Add to `/boot/firmware/config.txt`:
```ini
[all]
usb_max_current_enable=1
```

This allows USB ports to draw up to 1600mA instead of 600mA. Set it before connecting any USB peripherals.

Verify after reboot:
```bash
vcgencmd get_throttled   # expect 0x0 — no throttling or undervoltage events
```

### udev rules — static device names
```bash
# /etc/udev/rules.d/99-robot.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="303a", ATTRS{idProduct}=="1001", SYMLINK+="esp32"
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60",  SYMLINK+="rplidar"
```

```bash
# Reload udev
sudo udevadm control --reload-rules && sudo udevadm trigger
```

```bash
# Verify ROS 2 domain matches dev PC
export ROS_DOMAIN_ID=42
```
