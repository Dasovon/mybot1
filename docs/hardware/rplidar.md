# RPLIDAR — 2D LiDAR

## Role in This Project

The RPLIDAR provides 2D laser scan geometry used by `slam_toolbox` to build maps and by the Nav2 obstacle layer for collision avoidance. It is the primary geometry sensor for SLAM.

---

## Model — Confirmed

**RPLidar A1 M8** (Slamtec) — confirmed model in this build.

| Property | Value |
|---|---|
| Range | 0.15–12 m |
| Scan rate | ~5.5 Hz (confirmed on this build) |
| Points per scan | ~360 |
| Interface | USB via CP2102 adapter |

---

## Connection Notes

**Model:** RPLidar A1 M8 (Slamtec). Connects to Raspberry Pi via USB (CP2102 adapter, VID:10c4 PID:ea60).

| Property | Value |
|---|---|
| Interface | USB via CP2102 USB-serial adapter |
| Baud rate | **115200** — must be set explicitly (driver default in some versions is wrong) |
| udev symlink | `/dev/rplidar` |
| Power | Bus-powered via USB (5V, ~400 mA) |

### udev Rule

```
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="rplidar"
```

File: `/etc/udev/rules.d/99-robot.rules`

Reload: `sudo udevadm control --reload-rules && sudo udevadm trigger`

> ⚠️ Always set `serial_baudrate = 115200` explicitly in the launch file. Silent timeout if omitted.

---

## ROS 2 Driver

Package: `rplidar_ros`

```bash
sudo apt install ros-jazzy-rplidar-ros
```

Published topic: `/scan` (`sensor_msgs/LaserScan`)
Frame ID: `laser` (must match URDF `laser` link)

Run node directly (used in build testing and bringup):

```bash
ros2 run rplidar_ros rplidar_composition --ros-args \
    -p serial_port:=/dev/rplidar \
    -p serial_baudrate:=115200 \
    -p frame_id:=laser \
    -p angle_compensate:=true
```

> `serial_baudrate` must be set explicitly — the driver default in some versions is wrong and causes a silent timeout.

> `angle_compensate:=true` fills in scan gaps at low rotation speeds — always enable it.

### Verify scan health

```bash
ros2 topic hz /scan                       # expect ~5.5 Hz
ros2 topic echo /scan --once | head -30   # ranges must be non-zero, non-inf
```

### Motor control — known limitation

The `rplidar_ros` node controls the motor via the CP2102 USB adapter's DTR line. However, when the node exits and releases `/dev/rplidar`, the CP2102 may retain the last DTR state, leaving the motor running with no ROS process active.

**Tested on this build (2026-05-31):** All four DTR/RTS combinations were applied via pyserial while the port was held open. The motor did not change state in any combination. This indicates that either the DTR/RTS lines are not routed to MOTOCTL on this adapter revision, or the motor is powered continuously from USB 5V independent of the control line.

**Current status:** Stopping `mybot-lidar.service` halts `/scan` publishing and the ROS driver, but does not guarantee the physical motor stops. Unplugging the LiDAR USB cable is the reliable way to stop the motor during bench work.

**Long-term fix (not yet implemented):** A switched USB power path (load switch or USB hub with per-port power switching via `uhubctl`) controlled by a Pi GPIO would allow software-controlled motor power-off.

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

