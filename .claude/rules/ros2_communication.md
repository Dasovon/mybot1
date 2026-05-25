# ROS 2 Communication Rules — mybot1

## Topic naming conventions

- `snake_case` for all topic names
- Use namespace prefixes matching controller / hardware: `/diff_cont/`, `/imu/`, `/camera/`, `/battery_state`
- Never use leading slash in node code — ROS handles namespacing

## Authoritative topic list

| Topic | Type | Publisher | Subscriber | Rate |
|---|---|---|---|---|
| `/diff_cont/cmd_vel_unstamped` | `geometry_msgs/Twist` | Nav2 / twist_mux | ESP32 micro-ROS | 20 Hz |
| `/diff_cont/odom` | `nav_msgs/Odometry` | ESP32 micro-ROS | robot_localization EKF | 30 Hz |
| `/imu/imu` | `sensor_msgs/Imu` | ESP32 micro-ROS | robot_localization EKF | 30 Hz |
| `/battery_state` | `sensor_msgs/BatteryState` | ESP32 micro-ROS / serial bridge | monitoring | 1 Hz |
| `/odom` | `nav_msgs/Odometry` | robot_localization | Nav2, SLAM | 20 Hz |
| `/scan` | `sensor_msgs/LaserScan` | rplidar_node | slam_toolbox, Nav2 | ~5.5 Hz |
| `/camera/camera/depth/color/points` | `sensor_msgs/PointCloud2` | realsense2_camera | Nav2 voxel layer | ~13 Hz |
| `/camera/camera/color/image_raw` | `sensor_msgs/Image` | realsense2_camera | YOLO (future) | 15 Hz |
| `/camera/camera/depth/image_rect_raw` | `sensor_msgs/Image` | realsense2_camera | depth consumers | 15 Hz |

Do not rename these topics without updating: CLAUDE.md, the EKF config, Nav2 config, and all subscribers.

## QoS profiles — use these, never default

### Python
```python
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy

SENSOR_QOS = QoSProfile(
    reliability=ReliabilityPolicy.BEST_EFFORT,
    durability=DurabilityPolicy.VOLATILE,
    history=HistoryPolicy.KEEP_LAST,
    depth=10
)

CONTROL_QOS = QoSProfile(
    reliability=ReliabilityPolicy.RELIABLE,
    durability=DurabilityPolicy.VOLATILE,
    history=HistoryPolicy.KEEP_LAST,
    depth=10
)

STATE_QOS = QoSProfile(
    reliability=ReliabilityPolicy.RELIABLE,
    durability=DurabilityPolicy.TRANSIENT_LOCAL,
    history=HistoryPolicy.KEEP_LAST,
    depth=1
)
```

### C++
```cpp
#include <rclcpp/qos.hpp>

auto SENSOR_QOS = rclcpp::SensorDataQoS();

auto CONTROL_QOS = rclcpp::QoS(10)
    .reliable()
    .volatile_();

auto STATE_QOS = rclcpp::QoS(1)
    .reliable()
    .transient_local();
```

### When to use each
| Profile | Use for |
|---|---|
| `SENSOR_QOS` (BEST_EFFORT) | `/scan`, `/imu/imu`, `/diff_cont/odom`, `/camera/*` |
| `CONTROL_QOS` (RELIABLE/VOLATILE) | `/diff_cont/cmd_vel_unstamped`, `/battery_state` |
| `STATE_QOS` (RELIABLE/TRANSIENT_LOCAL) | `/robot_description`, latched status topics |

Publisher and subscriber QoS **must be compatible** or ROS will silently drop messages.

## TF2 frame tree (do not break this chain)

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

- `map → odom`: published by slam_toolbox
- `odom → base_link`: published by robot_localization EKF
- `base_link → *`: published by robot_state_publisher from URDF

TF2 lookup pattern:
```python
from tf2_ros import Buffer, TransformListener

self.tf_buffer = Buffer()
self.tf_listener = TransformListener(self.tf_buffer, self)

try:
    t = self.tf_buffer.lookup_transform('base_link', 'laser', rclpy.time.Time())
except Exception as e:
    self.get_logger().warn(f'TF lookup failed: {e}')
```

## Custom messages
All custom msgs/srvs/actions go in `src/robot_msgs/`. Follow this pattern:

```
# msg/BatteryStatus.msg
float32 voltage
float32 current
float32 power
bool sensors_ok
uint32 timestamp_ms
```

Import in Python: `from robot_msgs.msg import BatteryStatus`
Import in C++: `#include "robot_msgs/msg/battery_status.hpp"`

## Serial transport (ESP32 ↔ Pi) — dual port

| Port | ESP32 | Pi device | Purpose |
|---|---|---|---|
| Native USB CDC | GPIO 19/20 (built-in USB-JTAG, VID 303a:1001) | `/dev/ttyACM0` | micro-ROS transport + flashing (921600 baud) |
| CH340 UART0 | GPIO 43 TX / 44 RX (Lonely Binary on-board CH340, VID 1a86:7522) | `/dev/ttyUSB0` | Display telemetry JSON at 2 Hz (9600 baud) |

micro-ROS agent command:
```bash
source /opt/ros/jazzy/setup.bash && source ~/microros_ws/install/setup.bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -b 921600
```

CH340 JSON format from ESP32 (sent every 500 ms via Serial0.print):
```json
{"v":12.34,"i":1.23,"p":15.16,"ok":1,"ts":12345}
```

Monitor raw CH340 stream: `sudo cat /dev/ttyUSB0`
