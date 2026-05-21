# /phase

Check the current build phase, what's done, and what the next validation gate requires.

## Usage
`/phase`

## What this does

Reads `docs/architecture/build_plan.md` and reports:
1. Which phase is currently active (last phase marked complete)
2. The exact validation gate criteria for the current phase
3. The first step of the next phase (so you know what's coming)

## Quick phase reference

| Phase | Goal | Gate |
|-------|------|------|
| 0 | Hardware & environment | Pi SSH, ESP32 flash, LiDAR seen, RealSense seen |
| 1 | ESP32 firmware: PID, encoders, IMU, battery, micro-ROS, watchdog | Wheel velocity tracks command at 30 Hz; IMU publishes; battery cutoff works |
| 2 | URDF + TF tree | `view_frames` shows complete chain; no disconnected frames in RViz2 |
| 3 | Sensor bridge + LiDAR on `/scan` + RealSense + EKF → `/odom` | `/scan` at 5.5 Hz; `/odom` smooth under manual drive; LiDAR must pass before Phase 4 |
| 4 | SLAM: consistent 2D map | Map saved; no drift after 3 loops of test area |
| 5 | Nav2: autonomous navigation — MVP | Robot navigates to 3 waypoints without collision |
| 6 | BME680 + RealSense voxel costmap + display daemon | All sensors publishing; OLED live at boot |
| 7 | Semantic perception (YOLO) | YOLO detections published on `/detections` |

## Rules
- Do not start Phase N+1 until Phase N gate passes with hardware in the loop
- "Passes" means observed on real hardware, not just compiles
- Gate results go in `docs/testing/<phase>_validation_<YYYY-MM-DD>.md`

## To read full gate details
```bash
grep -A 20 "## Phase" ~/bot_ws/docs/architecture/build_plan.md
```
