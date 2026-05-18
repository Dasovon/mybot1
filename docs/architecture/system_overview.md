# System Architecture Overview

## Compute Layers

```
Development PC
(SLAM / Nav2 / AI / RViz / Logging)
        ↑
  Wi-Fi / Ethernet
        ↑
  Raspberry Pi 5
(Sensor bridge / ROS interface / drivers)
        ↑
    USB Serial
        ↑
    ESP32-S3
(Motion controller / telemetry / safety)
        ↑
    TB6612FNG
        ↑
      Motors
```

## Layer Responsibilities

| Layer | Hardware | Core Role |
|---|---|---|
| Embedded | ESP32-S3 | Real-time control, safety watchdog |
| Sensor bridge | Raspberry Pi 5 | ROS drivers, serial bridge |
| High-level | Development PC | SLAM, Nav2, AI, visualization |

## ROS 2 Topic Map

| Topic | Publisher | Consumer(s) |
|---|---|---|
| `/cmd_vel` | Nav2 | `esp32_serial_bridge` |
| `/odom` | `robot_localization` | Nav2, `slam_toolbox` |
| `/scan` | `rplidar_node` | `slam_toolbox`, Nav2 obstacle layer |
| `/camera/depth/points` | `realsense2_camera` | Nav2 voxel layer |
| `/imu` | `esp32_serial_bridge` | `robot_localization` |
| `/battery_state` | `esp32_serial_bridge` | Monitoring nodes |

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
