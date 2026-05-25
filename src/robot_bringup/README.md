# robot_bringup

Top-level launch coordinator for mybot1. Composes the full robot stack by including launch files from other packages. Also holds sensor-specific launch files that don't belong to a single subsystem package.

## Launch files

| File | Purpose | Run on |
|---|---|---|
| `realsense.launch.xml` | Starts `realsense2_camera` node (640×480 @ 15fps, RSUSB backend) | Pi |

Full robot bringup launch (Nav2 + SLAM + all sensors) will be added here in Phase 5.

## Launch

```bash
# RealSense camera only
ros2 launch robot_bringup realsense.launch.xml
```

## Config files

| File | Purpose |
|---|---|
| `config/realsense.yaml` | RealSense camera parameters: resolution, FPS, RSUSB backend, enabled streams |

## Dependencies

Pulls in the full stack at runtime:

| Package | Role |
|---|---|
| `esp32_serial_bridge` | ESP32 serial bridge (placeholder) |
| `robot_description` | URDF + TF publisher |
| `robot_navigation` | Nav2 config |
| `robot_slam` | SLAM Toolbox config |
| `rplidar_ros` | RPLidar A1 driver |
| `realsense2_camera` | Intel RealSense D435 driver |
| `robot_localization` | EKF — fuses odom + IMU → `/odom` |
