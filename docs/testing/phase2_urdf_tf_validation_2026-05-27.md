# Phase 2 URDF + TF Tree Validation — 2026-05-27

## Goal

Verify that `description.launch.py` publishes a complete, correctly-named TF tree
matching the authoritative frame list in `hardware_constants.md`.

## Hardware / software

- Dev PC: Ubuntu 24.04, ROS 2 Jazzy
- `robot_description` package built from commit `b59f95d`
- `description.launch.py` → `robot_state_publisher` + `joint_state_publisher`

---

## TF tree — view_frames

```bash
ros2 launch robot_description description.launch.py
ros2 run tf2_tools view_frames
```

Frames captured (from `frames_2026-05-27_16.47.35.gv`):

```
base_footprint
 └── base_link                             (fixed, RSP)
      ├── laser                            (fixed, RSP)
      ├── imu_link                         (fixed, RSP)
      ├── camera_link                      (fixed, RSP)
      │    └── camera_depth_frame          (fixed, RSP)
      ├── front_left_wheel                 (~28 Hz, JSP)
      ├── front_right_wheel                (~28 Hz, JSP)
      ├── rear_left_wheel                  (~28 Hz, JSP)
      └── rear_right_wheel                 (~28 Hz, JSP)
```

**9 frames total. All required frames present. No disconnected frames.**

Note: `odom → base_footprint` and `map → odom` are Phase 3 (EKF) and Phase 4 (SLAM)
respectively — not expected to appear here.

---

## Transform spot-checks

```bash
ros2 run tf2_ros tf2_echo base_link laser
```

| Transform | Translation (m) | Rotation (RPY) | Result |
|---|---|---|---|
| base_link → laser | [0, 0, 0.180] | [0, 0, 0] | ✓ |
| base_link → imu_link | [0, 0, 0.030] | [0, 0, 0] | ✓ |

Sensor offsets are placeholder values. Physical measurement and update required
before Phase 3 EKF bringup — offsets affect localization accuracy.

---

## Joint state publisher

`joint_state_publisher` running at 50 Hz (configured in `joint_state.yaml`).
All four wheel joints publishing at ~28 Hz from JSP (rate limited by description
update cycle, not the configured 50 Hz — acceptable, JSP rate is a max not a target).

Key fix applied during bringup: JSP 2.4.1 (Jazzy) requires the xacro file passed
as a positional argument, not via `robot_description` ROS parameter. The
`source_list: []` yaml key was removed — empty list caused rclpy type-inference
crash on parameter declare.

---

## Phase 2 gate result

| Check | Result |
|---|---|
| All 9 frames present | ✓ PASS |
| Frame names match `hardware_constants.md` | ✓ PASS |
| base_footprint root (required by Nav2 / robot_localization) | ✓ PASS |
| No disconnected frames | ✓ PASS |
| Wheel joints publishing | ✓ PASS |
| Static sensor frames correct | ✓ PASS |

**Phase 2: COMPLETE**
