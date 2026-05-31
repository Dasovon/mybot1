# ROS 2 Communication Rules — mybot1

## Topic naming conventions

- `snake_case` for all topic names
- Use namespace prefixes matching controller / hardware: `/diff_cont/`, `/imu/`, `/camera/`, `/battery_state`
- Never use leading slash in node code — ROS handles namespacing

## Authoritative topic list

| Topic | Publisher | Consumer | QoS | Rate |
|---|---|---|---|---:|
| `/diff_cont/cmd_vel_unstamped` | Nav2 / test script | ESP32 micro-ROS | RELIABLE / VOLATILE | 20 Hz |
| `/diff_cont/odom` | ESP32 micro-ROS | EKF / diagnostics | RELIABLE / VOLATILE | target 30 Hz |
| `/imu/imu` | ESP32 micro-ROS | EKF / diagnostics | RELIABLE / VOLATILE | target 30 Hz |
| `/battery_state` | Pi `battery_publisher` (INA219) | display / monitoring | RELIABLE / VOLATILE | 1 Hz |
| `/odom` | `robot_localization` | SLAM / Nav2 / diagnostics | RELIABLE / VOLATILE | configured 20 Hz; observed rate under investigation |
| `/scan` | RPLidar driver | SLAM / Nav2 | BEST_EFFORT / VOLATILE | ~5.5 Hz |

**QoS note — `/diff_cont/odom` and `/imu/imu`:** Both micro-ROS publishers are `rclc_publisher_init_default` (RELIABLE). `nav_msgs/Odometry` serializes to ~712 bytes, exceeding the 512-byte XRCE MTU; RELIABLE enables fragmentation. BEST_EFFORT silently drops oversized messages. Subscribers must use RELIABLE or a compatible profile — do not use SENSOR_QOS (BEST_EFFORT) for these topics.

**Rate monitoring rule:** Do not diagnose topic rates with default `ros2 topic hz` alone. Use explicitly compatible QoS and count received messages over a defined window. `/odom` currently observed at ~55 Hz externally after time-sync; root cause under investigation — do not assume 20 Hz until resolved.

Do not rename these topics without updating the EKF config, Nav2 config, and all subscribers.

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
| `SENSOR_QOS` (BEST_EFFORT) | `/scan`, `/camera/*` |
| `CONTROL_QOS` (RELIABLE/VOLATILE) | `/diff_cont/cmd_vel_unstamped`, `/diff_cont/odom`, `/imu/imu`, `/battery_state` |
| `STATE_QOS` (RELIABLE/TRANSIENT_LOCAL) | `/robot_description`, latched status topics |

Publisher and subscriber QoS **must be compatible** or ROS will silently drop messages.

## TF2 frame tree (do not break this chain)

```
map
 └── odom
      └── base_footprint          (EKF publishes odom → base_footprint)
           └── base_link          (URDF fixed joint: base_footprint → base_link)
                ├── laser
                ├── imu_link
                ├── camera_link
                │     └── camera_depth_frame
                ├── front_left_wheel
                ├── front_right_wheel
                ├── rear_left_wheel
                └── rear_right_wheel
```

- `map → odom`: published by slam_toolbox
- `odom → base_footprint`: published by robot_localization EKF
- `base_footprint → base_link`: URDF fixed joint, published by robot_state_publisher
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
| CH340 UART0 | GPIO 43 TX / 44 RX (Lonely Binary on-board CH340, VID 1a86:7522) | `/dev/ttyUSB0` | Debug console — firmware state, PID timing, micro-ROS transitions (115200 baud) |

micro-ROS agent command:
```bash
source /opt/ros/jazzy/setup.bash && source ~/microros_ws/install/setup.bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -b 921600
```

Monitor CH340 debug stream: `sudo cat /dev/ttyUSB0` (115200 baud)
