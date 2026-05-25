# robot_msgs

Custom ROS 2 message, service, and action definitions for mybot1. All custom interfaces that don't fit a standard `sensor_msgs` / `geometry_msgs` / `nav_msgs` type go here.

## Current state

No custom messages are defined yet. The package exists as a scaffold. Standard message types cover all current needs through Phase 5.

## Planned additions (Phase 6+)

| Type | Name | Purpose |
|---|---|---|
| msg | `EnvData.msg` | BME680 environmental sensor data (temperature, humidity, pressure, gas resistance) |

## Rules

- Never define custom messages inside other packages — all custom interfaces belong here.
- Prefer standard interfaces first. Only add a custom type when no standard type fits semantically.
- Use enum constants for action error codes, not freeform strings.

## Build

```bash
cd ~/bot_ws
colcon build --packages-select robot_msgs
source install/setup.bash
```
