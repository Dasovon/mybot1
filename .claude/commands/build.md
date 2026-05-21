# /build

Build the workspace or a specific package.

## Usage
`/build [package_name]`

## Commands

Build everything:
```bash
cd ~/bot_ws && colcon build --symlink-install
```

Build a single package:
```bash
cd ~/bot_ws && colcon build --symlink-install --packages-select <package_name>
```

Build with debug output:
```bash
cd ~/bot_ws && colcon build --symlink-install --packages-select <package_name> --cmake-args -DCMAKE_BUILD_TYPE=Debug
```

Source after build:
```bash
source ~/bot_ws/install/setup.bash
```

## Common errors

| Error | Fix |
|---|---|
| `package not found` | Check package.xml `<depend>` entries and `colcon build` again |
| `ImportError` at runtime | Missing entry point in setup.py or stale install — rebuild with `--symlink-install` |
| `CMake Error: could not find a package` | `apt install ros-humble-<package-name>` or add to package.xml |
| micro-ROS build errors | Ensure micro-ROS lib is built; check `~/microros_ws` |
