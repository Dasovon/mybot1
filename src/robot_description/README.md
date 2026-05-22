# robot_description

URDF model for mybot1: chassis geometry, wheel joints, and static sensor frames.

## Launch

```bash
ros2 launch robot_description description.launch.py
```

Starts `robot_state_publisher` (publishes TF from URDF) and `joint_state_publisher` (publishes zero wheel positions while motors are not active).

## Published transforms

| Parent | Child | Type | Notes |
|---|---|---|---|
| `base_link` | `left_wheel` | continuous joint | wheel axle, y = +0.0895 m |
| `base_link` | `right_wheel` | continuous joint | wheel axle, y = −0.0895 m |
| `base_link` | `laser` | fixed | RPLidar A1 — placeholder z = 0.18 m |
| `base_link` | `imu_link` | fixed | BNO055 — placeholder z = 0.03 m |
| `base_link` | `camera_link` | fixed | RealSense D435 — placeholder x = 0.09, z = 0.10 m |
| `camera_link` | `camera_depth_frame` | fixed | depth origin 15 mm left of color (D435 spec) |

## Files

| File | Purpose |
|---|---|
| `urdf/robot.urdf.xacro` | Top-level entry point |
| `urdf/base.xacro` | Chassis box + left/right wheel joints |
| `urdf/sensors.xacro` | Static sensor frames (laser, imu_link, camera) |
| `urdf/materials.xacro` | RViz2 colour definitions |
| `config/joint_state.yaml` | joint_state_publisher parameters |
| `launch/description.launch.py` | Launches robot_state_publisher + joint_state_publisher |

## Sensor offsets — update before Phase 3

The sensor positions in `sensors.xacro` are placeholders. Measure physical distances from the center of the wheel axle and update before EKF bringup:

- `laser`: x, y, z from `base_link` origin
- `imu_link`: x, y, z from `base_link` origin  
- `camera_link`: x, y, z from `base_link` origin; confirm yaw matches actual mount angle

Incorrect transforms cause EKF drift and distort Nav2 costmaps.

## Validation gate (Phase 2)

```bash
# Terminal 1
ros2 launch robot_description description.launch.py

# Terminal 2
ros2 run tf2_tools view_frames         # PDF must show full tree
ros2 run tf2_ros tf2_echo base_link laser   # must return a valid static transform
```

Expected TF tree:
```
base_link
├── left_wheel
├── right_wheel
├── laser
├── imu_link
└── camera_link
    └── camera_depth_frame
```
