# ROS 2 General Rules — mybot1

## Package naming
- Use `snake_case` for all package names
- Existing packages: `esp32_serial_bridge`, `robot_bringup`, `robot_description`, `robot_navigation`, `robot_slam`, `robot_msgs`
- Python packages use `ament_python`; C++ packages use `ament_cmake`

## File placement (enforced)
| What | Where |
|---|---|
| ROS node (Python) | `src/<package>/<package>/` |
| Launch file | `src/<package>/launch/` |
| YAML config | `src/<package>/config/` |
| Custom message | `src/robot_msgs/msg/` |
| Custom service | `src/robot_msgs/srv/` |
| Custom action | `src/robot_msgs/action/` |
| URDF / xacro | `src/robot_description/urdf/` |
| Mesh files | `src/robot_description/meshes/` |
| ESP32 firmware | `firmware/esp32/src/` |
| Hardware reference doc | `docs/hardware/` |
| Architecture doc | `docs/architecture/` |
| Test protocol / validation | `docs/testing/` |
| Shell utility scripts | `scripts/` |

## CMakeLists.txt (C++ packages)
```cmake
cmake_minimum_required(VERSION 3.8)
project(<package_name>)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
# add other dependencies here

add_executable(<node_name> src/<node_name>.cpp)
ament_target_dependencies(<node_name> rclcpp ...)

install(TARGETS <node_name> DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
```

## setup.py (Python packages)
```python
from setuptools import setup
import os
from glob import glob

package_name = '<package_name>'

setup(
    name=package_name,
    version='0.0.1',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        (os.path.join('share', package_name, 'launch'), glob('launch/*.py')),
        (os.path.join('share', package_name, 'config'), glob('config/*.yaml')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    entry_points={
        'console_scripts': [
            '<node_name> = <package_name>.<module_name>:main',
        ],
    },
)
```

## YAML parameter files
```yaml
/<node_name>:
  ros__parameters:
    param_name: value
    nested:
      param: value
```
Always namespace under the node name.

## Logging
```python
self.get_logger().debug('verbose detail')
self.get_logger().info('normal operation milestone')
self.get_logger().warn('recoverable issue')
self.get_logger().error('fault requiring attention')
self.get_logger().fatal('unrecoverable failure')
```
- Log at `info` on startup (node name, key params)
- Log at `warn` on degraded sensor / unexpected value
- Never log in a tight loop at `info` or above — use `debug`
