# /validate

Run pre-flight checks before a hardware test or commit. Catches QoS mismatches, build errors, and missing interfaces before they cause silent failures on the robot.

## Usage
`/validate [package_name]`

## Steps

### 1 — Build
```bash
cd ~/bot_ws && colcon build --symlink-install --packages-select <package_name>
# or: colcon build --symlink-install   (full workspace)
```

### 2 — Unit tests
```bash
colcon test --packages-select <package_name>
colcon test-result --verbose
```

### 3 — QoS compatibility check (when rosbridge or ros2-engineering-skills qos_checker is available)
```bash
# Check that pub/sub QoS profiles are compatible for key topics
ros2 topic info /diff_cont/odom --verbose
ros2 topic info /imu/imu --verbose
ros2 topic info /diff_cont/cmd_vel_unstamped --verbose
ros2 topic info /battery_state --verbose
```
Expected:
- `/diff_cont/odom`, `/imu/imu`, `/scan` → BEST_EFFORT publisher, BEST_EFFORT subscriber
- `/diff_cont/cmd_vel_unstamped`, `/battery_state` → RELIABLE publisher, RELIABLE subscriber
- Mismatched profiles = silent message drops

### 4 — TF tree
```bash
ros2 run tf2_tools view_frames
```
Expected chain: `map → odom → base_link → laser / imu_link / camera_link`

### 5 — Topic rates
```bash
ros2 topic hz /diff_cont/odom   # expect ~30 Hz
ros2 topic hz /imu/imu          # expect ~30 Hz
ros2 topic hz /scan             # expect ~5.5 Hz
ros2 topic hz /battery_state    # expect ~1 Hz
```

### 6 — Device presence (on Pi)
```bash
ls /dev/rplidar        # LiDAR symlink
ls /dev/ttyUSB0        # micro-ROS UART adapter
ls /dev/ttyACM0        # ESP32 Serial0 display telemetry
ls /dev/spidev0.0      # OLED SPI
```

## Pass criteria
- [ ] `colcon build` exits 0
- [ ] `colcon test` exits 0
- [ ] No QoS incompatibility warnings in `ros2 topic info`
- [ ] TF chain is complete with no disconnected frames
- [ ] All expected topic rates within 10% of target
- [ ] All device nodes present
