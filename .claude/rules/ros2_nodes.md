# ROS 2 Node Architecture — mybot1

## Core pattern: logic class separate from ROS node

Never put business logic in the Node class itself. The Node class handles only ROS wiring; a plain Python/C++ class holds the logic.

```python
# my_node.py
import rclpy
from rclpy.node import Node
from .my_controller import MyController  # pure logic, no ROS imports


class MyNode(Node):
    def __init__(self):
        super().__init__('my_node')
        self._declare_parameters()
        self.controller = MyController(
            self.get_parameter('param').value
        )
        self.sub = self.create_subscription(...)
        self.pub = self.create_publisher(...)
        self.timer = self.create_timer(0.05, self._timer_cb)
        self.get_logger().info('my_node started')

    def _declare_parameters(self):
        self.declare_parameter('param', 'default')

    def _timer_cb(self):
        result = self.controller.step()
        msg = ...
        self.pub.publish(msg)


def main(args=None):
    rclpy.init(args=args)
    node = MyNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
```

## Lifecycle nodes (use for sensor drivers and hardware interfaces)

Use `LifecycleNode` for any node that manages a hardware resource:
- `on_configure`: open serial port / device, load params
- `on_activate`: start publishing, start timers
- `on_deactivate`: stop publishing, stop timers (keep device open)
- `on_cleanup`: close device, release resources
- `on_shutdown`: full teardown

```python
from rclpy.lifecycle import LifecycleNode, TransitionCallbackReturn

class SensorNode(LifecycleNode):
    def on_configure(self, state):
        self.declare_parameter('device', '/dev/ttyUSB0')
        # open device here
        return TransitionCallbackReturn.SUCCESS

    def on_activate(self, state):
        self.timer = self.create_timer(0.1, self._publish)
        return TransitionCallbackReturn.SUCCESS

    def on_deactivate(self, state):
        self.destroy_timer(self.timer)
        return TransitionCallbackReturn.SUCCESS

    def on_cleanup(self, state):
        # close device
        return TransitionCallbackReturn.SUCCESS
```

## Timer rates — match system timing requirements

| Node | Rate | Timer period |
|---|---|---|
| EKF output consumer | 20 Hz | 0.050 s |
| IMU / odom publisher (micro-ROS) | 30 Hz | 0.033 s |
| Battery publisher | 1 Hz | 1.000 s |
| Nav2 controller | 20 Hz | 0.050 s |

## Executor selection

- Single node: `rclpy.spin(node)` (default SingleThreadedExecutor)
- Multiple nodes in one process: `MultiThreadedExecutor`
- Nodes with mutually exclusive callbacks: use `MutuallyExclusiveCallbackGroup`
- Nodes with concurrent callbacks: use `ReentrantCallbackGroup`

```python
from rclpy.executors import MultiThreadedExecutor
from rclpy.callback_groups import MutuallyExclusiveCallbackGroup

class MyNode(Node):
    def __init__(self):
        super().__init__('my_node')
        self.cb_group = MutuallyExclusiveCallbackGroup()
        self.sub = self.create_subscription(..., callback_group=self.cb_group)
        self.timer = self.create_timer(..., callback_group=self.cb_group)
```

## Do not
- Put logic directly in callbacks — dispatch to controller/domain class
- Block in callbacks (use async or separate thread for blocking I/O)
- Use `time.sleep()` inside a node — use `create_timer` instead
- Catch `Exception` broadly in callbacks — let ROS handle teardown
