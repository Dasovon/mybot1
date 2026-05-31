# System Architecture Overview

## Compute Layers

| Layer | Hardware | Core Role |
|---|---|---|
| Embedded | ESP32-S3 | Real-time PID control, safety watchdog, micro-ROS publisher |
| Sensor bridge | Raspberry Pi 5 | ROS drivers, micro-ROS agent, EKF, robot_state_publisher |
| High-level | Development PC | SLAM, Nav2, RViz2, YOLO |

## ROS 2 Topic Map

| Topic | Publisher | Consumer(s) | Rate |
|---|---|---|---|
| `/diff_cont/cmd_vel_unstamped` | Nav2 / dev PC | ESP32 via micro-ROS | 20 Hz |
| `/diff_cont/odom` | ESP32 via micro-ROS | robot_localization EKF | 30 Hz |
| `/imu/imu` | ESP32 via micro-ROS | robot_localization EKF | 30 Hz |
| `/battery_state` | Pi `battery_publisher` (INA219) | monitoring, display daemon | 1 Hz |
| `/odom` | robot_localization EKF | Nav2, slam_toolbox | configured 20 Hz; observed rate under investigation |
| `/scan` | rplidar_node | slam_toolbox, Nav2 obstacle layer | ~5.5 Hz |
| `/camera/camera/depth/color/points` | realsense2_camera | Nav2 voxel layer | ~6 Hz |
| `/camera/camera/color/image_raw` | realsense2_camera | YOLO (Phase 7) | 15 Hz |

## TF Tree

```
map                          ← slam_toolbox (Phase 4+)
 └── odom                    ← robot_localization EKF (Phase 3+)
      └── base_footprint     ← robot_localization EKF
           └── base_link     ← robot_state_publisher (URDF fixed joint)
                ├── laser
                ├── imu_link
                ├── camera_link
                │    └── camera_depth_frame
                ├── front_left_wheel
                ├── front_right_wheel
                ├── rear_left_wheel
                └── rear_right_wheel
```

## Phase Status

| Phase | Goal | Status |
|---|---|---|
| 0 | Hardware & environment | **Complete** |
| 1 | ESP32 firmware: PID, encoders, IMU, micro-ROS, watchdog | **Complete** — P-only baseline validated; watchdog source changed to 500 ms, pending flash validation |
| 2 | URDF + TF tree | **Complete** — 9 frames validated |
| 3 | EKF + LiDAR → `/odom` | **In progress** — EKF live; `/odom` rate anomaly under investigation; LiDAR pending |
| 4 | SLAM: consistent 2D map | Not started |
| 5 | Nav2: autonomous navigation | Not started |
| 6 | BME680 + RealSense voxel costmap | Not started |
| 7 | Semantic perception (YOLO) | Not started |

## Safety Rule

```
Dev PC failure ≠ dangerous robot
```

ESP32 watchdog is `WATCHDOG_MS = 500` ms in source — pending flash and stop-time validation. Battery low-voltage cutoff runs on the **Pi** (`battery_publisher` node), not the ESP32 — it is a ROS publisher race currently and must be replaced by priority arbitration before Nav2.
