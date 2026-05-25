# robot_slam

SLAM configuration and launch files for mybot1. Current implementation: `slam_toolbox` (2D lidar SLAM). Future: RTAB-Map (RGB-D loop closure, Phase 6+).

## Current state

Scaffold only — no config files or launch files yet. SLAM bringup is Phase 4.

## Planned contents (Phase 4)

| File | Purpose |
|---|---|
| `config/slam_toolbox_params.yaml` | slam_toolbox parameters: map resolution, scan topic, odom frame |
| `launch/slam.launch.xml` | Starts `slam_toolbox` in online async mode |

## SLAM stack

| Phase | Tool | Mode |
|---|---|---|
| Phase 4–5 | `slam_toolbox` | Online async mapping |
| Phase 6+ | RTAB-Map | RGB-D + LiDAR loop closure |

## Key topics

| Topic | Type | Role |
|---|---|---|
| `/scan` | `sensor_msgs/LaserScan` | Input — RPLidar A1 at ~5.5 Hz |
| `/odom` | `nav_msgs/Odometry` | Input — EKF-fused odometry at 20 Hz |
| `/map` | `nav_msgs/OccupancyGrid` | Output — 2D occupancy map |
| `/tf` | `tf2_msgs/TFMessage` | Output — `map → odom` transform |

## Run on

SLAM runs on the **development PC** (GPU preferred for RTAB-Map). The Pi streams `/scan` and `/odom`; the dev PC builds the map.

## Save / load map

```bash
# Save
ros2 run nav2_map_server map_saver_cli -f ~/maps/mybot1_map

# Load for Nav2
ros2 run nav2_map_server map_server --ros-args -p map:=~/maps/mybot1_map.yaml
```
