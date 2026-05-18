# RPLIDAR — 2D LiDAR

## Role in This Project

The RPLIDAR provides 2D laser scan geometry used by `slam_toolbox` to build maps and by the Nav2 obstacle layer for collision avoidance. It is the primary geometry sensor for SLAM.

---

## Common Models

| Model | Range | Scan Rate | Points/Scan |
|---|---|---|---|
| A1M8 | 0.15–12 m | 5–10 Hz | ~360 |
| A2M8 | 0.15–18 m | 10 Hz | ~720 |
| A3M1 | 0.2–25 m | 10–15 Hz | ~16,000 |
| C1 | 0.1–12 m | 10 Hz | ~7,200 |
| S1 | 0.1–40 m | 10 Hz | ~8,192 |

Update this file with your specific model once confirmed.

---

## Wiring — Confirmed Connections

**Model confirmed:** RPLidar A1 M8 (Slamtec).

```
RPLidar A1 M8
    └── USB adapter (CP2102, VID:10c4 PID:ea60)
            └── Raspberry Pi USB 2.0 port
                    /dev/rplidar  (udev symlink)
```

| Property | Value |
|---|---|
| Interface | USB (via CP2102 USB-serial adapter) |
| Connected to | Raspberry Pi 5 USB 2.0 port |
| Baud rate | **115200** (must be set explicitly — default in some driver versions is wrong) |
| udev symlink | `/dev/rplidar` |
| Power | Bus-powered via USB (5V, ~400 mA) |

### udev Rule

```
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="rplidar"
```

File: `/etc/udev/rules.d/99-mybot.rules`

Reload: `sudo udevadm control --reload-rules && sudo udevadm trigger`

> ⚠️ Always set `serial_baudrate = 115200` explicitly in the launch file. Silent timeout if omitted.

---

## ROS 2 Driver

Package: `rplidar_ros`

```bash
sudo apt install ros-humble-rplidar-ros
```

Published topic: `/scan` (`sensor_msgs/LaserScan`)
Frame: `laser` (must match URDF)

Launch example:

```bash
ros2 launch rplidar_ros rplidar_launch.py \
  serial_port:=/dev/rplidar \
  frame_id:=laser
```

---

## Known Issue — Low Mounting Position

The LiDAR is mounted low to the ground. At this height it detects:
- Chair legs
- Shoe edges
- Box corners
- Furniture bases

These appear as walls in the SLAM map. Mitigation:
- Use RealSense D435 depth data to validate LiDAR geometry.
- Use `slam_toolbox`'s inflation radius to prevent false narrow passages.
- In Nav2 costmaps: LiDAR feeds the **obstacle layer** only; the RealSense feeds the **voxel layer** for 3D context.

---

## TF Frame

The LiDAR frame `laser` must be defined in the URDF relative to `base_link`.

```
base_link → laser
```

The transform must accurately reflect the physical mounting position and orientation (especially the yaw if the LiDAR is rotated).

---

## Scan Parameters in slam_toolbox / Nav2

Key parameters to tune:

| Parameter | Typical Value | Notes |
|---|---|---|
| `min_range` | 0.15 m | Filter out close reflections |
| `max_range` | 12.0 m | Match sensor spec |
| `angle_min` | -π | Full 360° scan |
| `angle_max` | π | Full 360° scan |

---

## udev Rule

```
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="rplidar"
```

Reload with:

```bash
sudo udevadm control --reload-rules && sudo udevadm trigger
```
