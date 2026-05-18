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
| OS | Ubuntu 22.04 LTS (64-bit), ROS 2 Humble |

---

## Wiring — Current Connections (from spec)

| Connects To | Interface | Purpose |
|---|---|---|
| ESP32-S3 | USB Serial | Command/telemetry bridge |
| RPLIDAR | USB | 2D LiDAR data |
| RealSense D435 | USB | RGB-D depth camera |
| Development PC | Wi-Fi or Ethernet | ROS 2 network |

### Common Ground

Pi GND must connect to: Battery −, ESP32 GND, TB6612 GND, and all sensor GNDs.

> Specific USB port assignments (which port hosts which device) are TBD.

---

## ROS 2 Nodes Running on Pi

| Node | Package | Topic(s) |
|---|---|---|
| `esp32_serial_bridge` | `esp32_serial_bridge` | `/odom`, `/imu`, `/battery_state`, `/cmd_vel` |
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
- The Pi 5 draws significantly more power than Pi 4 under load
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

## Setup Notes

```bash
# Set static device names via udev
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
