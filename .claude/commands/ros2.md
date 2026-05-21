# /ros2

Quick reference for ROS 2 debugging on this robot.

## Devices (on Pi)
```bash
ls /dev/rplidar
ls /dev/ttyUSB0                          # micro-ROS UART
ls /dev/ttyACM0                          # display telemetry Serial0
ls /dev/serial/by-id/usb-Espressif*
```

## Start micro-ROS agent
```bash
source /opt/ros/humble/setup.bash
source ~/microros_ws/install/setup.bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyUSB0 -b 115200
```

## Topic health
```bash
ros2 topic hz /diff_cont/odom
ros2 topic hz /imu/imu
ros2 topic hz /scan
ros2 topic echo /battery_state
ros2 topic list
```

## TF
```bash
ros2 run tf2_tools view_frames
ros2 run tf2_ros tf2_echo base_link laser
ros2 run tf2_ros tf2_echo odom base_link
```

## Display daemon
```bash
# Monitor raw Serial0 JSON from ESP32
cat /dev/ttyACM0
# Service management
sudo systemctl status mybot-display.service
sudo systemctl restart mybot-display.service
sudo journalctl -u mybot-display.service -f
```

## LiDAR
```bash
# Fix stale process locking /dev/rplidar
sudo fuser -k /dev/rplidar
# Check LiDAR is publishing
ros2 topic hz /scan
```

## Robot launch service
```bash
sudo systemctl restart robot-launch.service
sudo journalctl -u robot-launch.service -f
```

## Build workspace
```bash
cd ~/bot_ws && colcon build --symlink-install
source ~/bot_ws/install/setup.bash
```
