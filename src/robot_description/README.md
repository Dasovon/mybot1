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
| `base_footprint` | `base_link` | fixed | Root-to-chassis (identity, required by Nav2 / robot_localization) |
| `base_link` | `front_left_wheel` | continuous joint | y = +0.0885 m |
| `base_link` | `front_right_wheel` | continuous joint | y = −0.0885 m |
| `base_link` | `rear_left_wheel` | continuous joint | y = +0.0885 m |
| `base_link` | `rear_right_wheel` | continuous joint | y = −0.0885 m |
| `base_link` | `laser` | fixed | RPLidar A1 — z = 0.180 m (placeholder, measure before EKF) |
| `base_link` | `imu_link` | fixed | BNO055 — z = 0.030 m (placeholder) |
| `base_link` | `camera_link` | fixed | RealSense D435 — placeholder offsets |
| `camera_link` | `camera_depth_frame` | fixed | depth origin 15 mm left of color (D435 spec) |

## Files

| File | Purpose |
|---|---|
| `urdf/robot.urdf.xacro` | Top-level entry point |
| `urdf/base.xacro` | base_footprint root + chassis box + 4 wheel joints |
| `urdf/sensors.xacro` | Static sensor frames (laser, imu_link, camera) |
| `urdf/materials.xacro` | RViz2 colour definitions |
| `config/joint_state.yaml` | joint_state_publisher rate (50 Hz) |
| `launch/description.launch.py` | Launches robot_state_publisher + joint_state_publisher |

## Pending

- **Sensor offsets** in `sensors.xacro`: laser z=0.180, imu z=0.030, camera offsets are all placeholders. Measure physical positions before Phase 3 EKF bringup — offsets affect localization accuracy.
- **Wheelbase** in `base.xacro`: 0.18 m is a placeholder. Measure front-to-rear axle distance when final chassis is ready.

## Phase 2 gate — COMPLETE (2026-05-27)

```bash
ros2 launch robot_description description.launch.py
ros2 run tf2_tools view_frames
```

Validated TF tree (all 9 frames connected, no disconnected frames):
```
base_footprint
 └── base_link
      ├── front_left_wheel    (~28 Hz, JSP)
      ├── front_right_wheel   (~28 Hz, JSP)
      ├── rear_left_wheel     (~28 Hz, JSP)
      ├── rear_right_wheel    (~28 Hz, JSP)
      ├── laser               (fixed, RSP)
      ├── imu_link            (fixed, RSP)
      └── camera_link         (fixed, RSP)
           └── camera_depth_frame  (fixed, RSP)
```

Spot-checked transforms:
- `tf2_echo base_link laser` → [0, 0, 0.180]
- `tf2_echo base_link imu_link` → [0, 0, 0.030]

See [`docs/testing/phase2_urdf_tf_validation_2026-05-27.md`](../../docs/testing/phase2_urdf_tf_validation_2026-05-27.md) for full results.
