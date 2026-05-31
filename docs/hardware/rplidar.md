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

### Motor control — verified behavior

The RPLidar A1 motor is controlled through the CP2102 USB adapter's DTR line.

**Tested on this build (2026-05-31):**

| Serial state | Motor behavior |
|---|---|
| `DTR=False, RTS=False` | **Stopped** |
| `DTR=True,  RTS=False` | Running |
| `DTR=False, RTS=True`  | **Stopped** |
| `DTR=True,  RTS=True`  | Running |

RTS has no observed effect. `DTR=False` stops the motor; `DTR=True` runs it.

When `/dev/rplidar` is released and the port closes, the CP2102 reverts to the motor-running state. Stopping `mybot-lidar.service` alone stops `/scan` publishing but does not physically stop the motor.

**Implemented solution:** `mybot-lidar-motor-off.service` runs `lidar_motor_off.py`, which persistently holds `/dev/rplidar` open with `DTR=False` whenever the ROS LiDAR driver is not active. The two services conflict — starting one stops the other.

| Mode | Active service | Motor | `/scan` |
|---|---|---|---|
| Bench / idle | `mybot-lidar-motor-off.service` | Off | Not publishing |
| SLAM / scan | `mybot-lidar.service` | Running | Publishing |

```bash
# Start scanning (stops motor-off holder automatically via Conflicts=):
sudo systemctl start mybot-lidar.service

# Return to motor-off idle (ExecStopPost on mybot-lidar.service handles this):
sudo systemctl stop mybot-lidar.service
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

