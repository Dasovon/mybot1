# robot_description

URDF model for mybot1: chassis geometry, 4-wheel skid steer joints, and static sensor frames.

## Launch

```bash
ros2 launch robot_description description.launch.py
```

Starts `robot_state_publisher` (publishes TF from URDF) and `joint_state_publisher` (publishes zero wheel positions while motors are not active).

## Published transforms

| Parent | Child | Type | Notes |
|---|---|---|---|
| `base_link` | `front_left_wheel` | continuous joint | y = +0.0885 m, x = +wheelbase/2 |
| `base_link` | `front_right_wheel` | continuous joint | y = −0.0885 m, x = +wheelbase/2 |
| `base_link` | `rear_left_wheel` | continuous joint | y = +0.0885 m, x = −wheelbase/2 |
| `base_link` | `rear_right_wheel` | continuous joint | y = −0.0885 m, x = −wheelbase/2 |
| `base_link` | `laser` | fixed | RPLidar A1 — placeholder offsets |
| `base_link` | `imu_link` | fixed | BNO055 — placeholder offsets |
| `base_link` | `camera_link` | fixed | RealSense D435 — placeholder offsets |
| `camera_link` | `camera_depth_frame` | fixed | depth origin 15 mm left of color (D435 spec) |

## Files

| File | Purpose |
|---|---|
| `urdf/robot.urdf.xacro` | Top-level entry point |
| `urdf/base.xacro` | Chassis box + 4 wheel joints (skid steer) |
| `urdf/sensors.xacro` | Static sensor frames (laser, imu_link, camera) |
| `urdf/materials.xacro` | RViz2 colour definitions |
| `config/joint_state.yaml` | joint_state_publisher parameters |
| `launch/description.launch.py` | Launches robot_state_publisher + joint_state_publisher |

## Pending — update when hardware arrives

- **`wheelbase`** in `base.xacro`: placeholder 0.18 m — measure front-to-rear axle distance on new chassis
- **Sensor offsets** in `sensors.xacro`: all placeholder — measure physical positions before Phase 3 EKF bringup

## Validation gate (Phase 2)

```bash
# Terminal 1
ros2 launch robot_description description.launch.py

# Terminal 2
ros2 run tf2_tools view_frames
ros2 run tf2_ros tf2_echo base_link laser
```

Expected TF tree:
```
base_link
├── front_left_wheel
├── front_right_wheel
├── rear_left_wheel
├── rear_right_wheel
├── laser
├── imu_link
└── camera_link
    └── camera_depth_frame
```
