# Development PC — High-Level Compute / Mission Control

## Role in This Project

The development PC is the "brain" of the robot. It runs all computationally intensive workloads: SLAM, navigation planning, AI inference, visualization, and logging. The robot must remain safe if this machine disconnects.

---

## Minimum Recommended Specs

| Component | Minimum | Recommended |
|---|---|---|
| CPU | Modern quad-core | 8+ core (e.g., Intel i7/i9, AMD Ryzen 7/9) |
| RAM | 16 GB | 32 GB |
| GPU | Any NVIDIA (for CUDA) | NVIDIA RTX series |
| Storage | 256 GB SSD | 1 TB NVMe SSD |
| Networking | Wi-Fi | Gigabit Ethernet (preferred for ROS DDS) |
| OS | Ubuntu 22.04 LTS | Ubuntu 22.04 LTS |

---

## Software Stack

| Software | Version | Purpose |
|---|---|---|
| Ubuntu | 22.04 LTS | Operating system |
| ROS 2 | Humble Hawksbill | Robot middleware |
| slam_toolbox | Latest Humble | 2D SLAM |
| RTAB-Map | Latest Humble | Future: RGB-D SLAM |
| Nav2 | Latest Humble | Autonomous navigation |
| RViz2 | With Humble | Visualization |
| robot_localization | Latest Humble | EKF sensor fusion |
| YOLO (Ultralytics) | v8+ | Future: semantic perception |
| CUDA | 11.8+ / 12.x | GPU acceleration for AI |

---

## ROS 2 Nodes Running on Dev PC

| Node | Package | Purpose |
|---|---|---|
| `slam_toolbox_node` | `slam_toolbox` | 2D occupancy map |
| `controller_server` | `nav2_controller` | Path following |
| `planner_server` | `nav2_planner` | Global path planning |
| `bt_navigator` | `nav2_bt_navigator` | Behavior tree navigation |
| `costmap_2d` | `nav2_costmap_2d` | Multi-layer costmap |
| `ekf_node` | `robot_localization` | Odometry + IMU fusion |
| `rviz2` | `rviz2` | Visualization |
| `rosbag2` | `rosbag2` | Data recording |

---

## Networking with Raspberry Pi

| Setting | Value |
|---|---|
| ROS_DOMAIN_ID | Must match Pi (e.g., `42`) |
| DDS | CycloneDDS or FastDDS |
| Preferred connection | Ethernet (Gigabit) |
| Fallback | Wi-Fi 5 GHz |

```bash
# .bashrc on both Dev PC and Pi
export ROS_DOMAIN_ID=42
export CYCLONEDDS_URI=file:///path/to/cyclone_config.xml
```

---

## GPU — CUDA Setup

For YOLO inference and point cloud processing:

```bash
# Verify CUDA
nvidia-smi
nvcc --version

# Install CUDA-enabled PyTorch (for YOLO)
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu121
```

---

## rosbag Recording

Record all sensor data for offline development and debugging:

```bash
ros2 bag record \
  /scan \
  /camera/depth/points \
  /camera/color/image_raw \
  /imu/imu \
  /odom \
  /battery_state \
  /tf \
  /tf_static \
  -o robot_session_$(date +%Y%m%d_%H%M%S)
```

---

## Safety Constraint

The robot must remain safe if this PC:
- Loses Wi-Fi
- Crashes or is powered off
- Fails to send velocity commands

The ESP32 watchdog handles this — it stops motors on command timeout. **Do not rely on the dev PC for safety-critical behavior.**
