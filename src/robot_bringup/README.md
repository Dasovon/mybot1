# robot_bringup

Top-level launch coordinator for mybot1. Holds sensor launch files and the EKF launch. Full robot bringup (Nav2 + SLAM + all sensors) will be added here in Phase 5.

## Launch files

| File | Purpose | Run on |
|---|---|---|
| `realsense.launch.xml` | Starts `realsense2_camera` node (424×240 depth @ 6 Hz, color @ 15 Hz, RGB8 point cloud) | Pi |
| `lidar.launch.xml` | Starts `rplidar_ros` on `/dev/rplidar` — publishes `/scan` at ~5.5 Hz, frame `laser` | Pi |
| `ekf.launch.xml` | Starts `robot_localization` EKF — fuses `/diff_cont/odom` + `/imu/imu` → `/odom` at 20 Hz | Pi |

## Launch

```bash
# RealSense camera only
ros2 launch robot_bringup realsense.launch.xml

# LiDAR only
ros2 launch robot_bringup lidar.launch.xml

# EKF only (requires microros-agent.service running and ESP32 publishing)
ros2 launch robot_bringup ekf.launch.xml

# Phase 3 full sensor stack
ros2 launch robot_description description.launch.py &
ros2 launch robot_bringup lidar.launch.xml &
ros2 launch robot_bringup realsense.launch.xml &
ros2 launch robot_bringup ekf.launch.xml
```

## Config files

| File | Purpose |
|---|---|
| `config/realsense.yaml` | RealSense: 424×240 depth @ 6 Hz / color @ 15 Hz, RGB8 point cloud, align_depth=true |
| `config/ekf.yaml` | robot_localization EKF: inputs, covariance matrices, two_d_mode=true |

## EKF configuration summary

Inputs:
- `/diff_cont/odom` → x, y, yaw position + vx, vyaw velocity
- `/imu/imu` → angular velocity z + linear acceleration x/y (orientation excluded — BNO055 magnetometer unreliable on metal chassis)

Output: `/odom` at 20 Hz (remapped from `odometry/filtered`), publishes `odom → base_footprint` TF.

## Phase status

| Phase | Gate | Status |
|---|---|---|
| Phase 3 — EKF | `/odom` at 20 Hz, `odom→base_footprint` TF live | EKF: **PASS** (2026-05-27) |
| Phase 3 — LiDAR | `/scan` at ~5.5 Hz, non-zero ranges | LiDAR: not yet validated (sensor not connected) |

## Dependencies

| Package | Role |
|---|---|
| `robot_localization` | EKF sensor fusion |
| `rplidar_ros` | RPLidar A1 driver |
| `realsense2_camera` | Intel RealSense D435 driver |
