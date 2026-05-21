# Clean Architecture — mybot1 ROS 2 Packages

## Four-layer structure

```
src/<package>/
├── domain/          # pure Python/C++, zero ROS imports
│   ├── models.py    # data classes (no ROS message types)
│   └── interfaces.py  # abstract base classes
├── application/     # use-case logic, orchestrates domain
│   └── <feature>_service.py
├── infrastructure/  # ROS 2 wiring, hardware drivers, serialization
│   ├── ros/
│   │   └── <node_name>_node.py
│   └── drivers/
│       └── <hardware>_driver.py
└── presentation/    # entry points only
    └── main.py
```

## Domain layer — no ROS imports ever

```python
# domain/robot_state.py
from dataclasses import dataclass

@dataclass
class RobotState:
    x: float
    y: float
    theta: float
    v_left: float
    v_right: float
    battery_voltage: float
```

```python
# domain/interfaces.py
from abc import ABC, abstractmethod
from .robot_state import RobotState

class MotorController(ABC):
    @abstractmethod
    def set_velocity(self, left: float, right: float) -> None: ...

    @abstractmethod
    def get_state(self) -> RobotState: ...
```

## Application layer — orchestrates domain, still no ROS

```python
# application/navigation_service.py
from ..domain.interfaces import MotorController
from ..domain.robot_state import RobotState

class NavigationService:
    def __init__(self, controller: MotorController):
        self._controller = controller
        self._target_v = 0.0
        self._target_w = 0.0

    def set_cmd_vel(self, linear: float, angular: float) -> None:
        self._target_v = linear
        self._target_w = angular

    def step(self) -> RobotState:
        wheel_separation = 0.179
        v_left = self._target_v - self._target_w * wheel_separation / 2
        v_right = self._target_v + self._target_w * wheel_separation / 2
        self._controller.set_velocity(v_left, v_right)
        return self._controller.get_state()
```

## Infrastructure layer — all ROS code lives here

```python
# infrastructure/ros/navigation_node.py
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
from nav_msgs.msg import Odometry
from ...application.navigation_service import NavigationService
from ..drivers.serial_motor_driver import SerialMotorDriver
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy

SENSOR_QOS = QoSProfile(
    reliability=ReliabilityPolicy.BEST_EFFORT,
    durability=DurabilityPolicy.VOLATILE,
    history=HistoryPolicy.KEEP_LAST,
    depth=10
)

class NavigationNode(Node):
    def __init__(self):
        super().__init__('navigation_node')
        driver = SerialMotorDriver('/dev/ttyUSB0')
        self.service = NavigationService(driver)
        self.sub = self.create_subscription(
            Twist, '/diff_cont/cmd_vel_unstamped', self._cmd_cb, SENSOR_QOS
        )
        self.pub = self.create_publisher(Odometry, '/diff_cont/odom', SENSOR_QOS)
        self.timer = self.create_timer(1/30, self._publish_odom)

    def _cmd_cb(self, msg: Twist):
        self.service.set_cmd_vel(msg.linear.x, msg.angular.z)

    def _publish_odom(self):
        state = self.service.step()
        # convert state → Odometry msg and publish
```

## Anti-patterns to avoid

- ROS imports in domain/ or application/ layers
- `sensor_msgs/Imu` or any ROS message type as a domain model
- Node class containing PID logic, filter math, or coordinate transforms
- Calling `rclpy.spin()` anywhere except the entry point
- Global state or singletons — pass dependencies explicitly
