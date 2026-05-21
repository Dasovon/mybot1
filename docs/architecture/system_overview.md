# System Architecture Overview

## Compute Layers

![System Architecture](system_overview.png)

## Layer Responsibilities

| Layer | Hardware | Core Role |
|---|---|---|
| Embedded | ESP32-S3 | Real-time control, safety watchdog |
| Sensor bridge | Raspberry Pi 5 | ROS drivers, serial bridge |
| High-level | Development PC | SLAM, Nav2, AI, visualization |

## ROS 2 Topic Map

| Topic | Publisher | Consumer(s) |
|---|---|---|
| `/diff_cont/cmd_vel_unstamped` | Nav2 | ESP32 via micro-ROS |
| `/diff_cont/odom` | ESP32 via micro-ROS | `robot_localization` EKF |
| `/odom` | `robot_localization` | Nav2, `slam_toolbox` |
| `/scan` | `rplidar_node` | `slam_toolbox`, Nav2 obstacle layer |
| `/camera/depth/points` | `realsense2_camera` | Nav2 voxel layer |
| `/imu/imu` | ESP32 via micro-ROS | `robot_localization` EKF |
| `/battery_state` | ESP32 via micro-ROS | Monitoring nodes |

## TF Tree

```
map
 └── odom
      └── base_link
           ├── laser
           ├── imu_link
           ├── camera_link
           │     └── camera_depth_frame
           ├── left_wheel
           └── right_wheel
```

## Safety Rule

```
Dev PC failure ≠ dangerous robot
```

The ESP32 watchdog stops motors on command timeout. Battery cutoff also runs on ESP32, independent of ROS.
