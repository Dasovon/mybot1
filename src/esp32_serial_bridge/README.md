# esp32_serial_bridge

Placeholder ROS 2 Python package for bridging the ESP32-S3 to the Pi. In the current architecture (Phases 1–5), the ESP32 communicates via micro-ROS directly — this package's node is not yet needed. It will be used in Phase 6+ if a serial parsing node is required for non-micro-ROS data streams (e.g. BME680 JSON over UART).

## Current state

No nodes are implemented. The package exists as a scaffold with the correct ament_python structure.

## Planned topics (Phase 6+)

| Topic | Type | Direction | Rate |
|---|---|---|---|
| `/battery_state` | `sensor_msgs/BatteryState` | publish | 1 Hz |

In Phases 1–5, `/battery_state` is published directly by the ESP32 micro-ROS node. This package would only take over that role if the micro-ROS transport is replaced with a raw serial protocol.

## Launch

No launch file yet. Will be added when a node is implemented.

## Dependencies

| Package | Reason |
|---|---|
| `rclpy` | ROS 2 Python client library |
| `sensor_msgs` | `BatteryState` message type |
| `nav_msgs` | `Odometry` message type (planned) |
| `geometry_msgs` | `Twist` message type (planned) |
| `robot_msgs` | Custom message types |
