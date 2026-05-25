# robot_navigation

Nav2 configuration and launch files for mybot1. Holds costmap parameters, planner config, controller config, and the Nav2 bringup launch file.

## Current state

Scaffold only — no config files or launch files yet. Nav2 bringup is Phase 5.

## Planned contents (Phase 5)

| File | Purpose |
|---|---|
| `config/nav2_params.yaml` | Full Nav2 parameter file: costmap layers, planner, controller, BT navigator |
| `launch/navigation.launch.xml` | Starts Nav2 stack with map server, costmap, planner, controller |

## Nav2 stack overview

| Layer | Plugin | Data source |
|---|---|---|
| Static layer | `nav2_costmap_2d::StaticLayer` | SLAM map from `slam_toolbox` |
| Obstacle layer | `nav2_costmap_2d::ObstacleLayer` | `/scan` (RPLidar A1) |
| Voxel layer | `nav2_costmap_2d::VoxelLayer` | `/camera/depth/points` (RealSense, Phase 6) |

## Key Nav2 settings for this robot

- `enable_stamped_cmd_vel: false` — ESP32 uses `geometry_msgs/Twist`, not `TwistStamped`
- Controller frequency: 20 Hz
- Planner: NavFn (default) → swap to Smac Hybrid-A* if needed for tight spaces

## Run on

Nav2 runs on the **development PC**, not the Pi. The Pi runs only the sensor drivers, micro-ROS agent, EKF, and `robot_state_publisher`.
