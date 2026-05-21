# /new-package

Create a new ROS 2 package under `src/`.

## Usage
`/new-package <package_name> [python|cmake]`

## Steps

1. Confirm the package name is `snake_case` and doesn't conflict with existing packages
2. For Python packages (`ament_python`):
   ```bash
   cd ~/bot_ws/src
   ros2 pkg create --build-type ament_python <package_name>
   ```
3. For C++ packages (`ament_cmake`):
   ```bash
   cd ~/bot_ws/src
   ros2 pkg create --build-type ament_cmake <package_name>
   ```
4. Create the standard directory layout:
   ```
   src/<package_name>/
   ├── <package_name>/   # Python: module source
   ├── launch/
   ├── config/
   ├── test/
   ├── package.xml
   └── setup.py (Python) / CMakeLists.txt (C++)
   ```
5. Update `package.xml` with correct dependencies
6. Set up `setup.py` data_files to install launch/ and config/ (Python packages)
7. Follow templates in `.claude/rules/ros2_general.md`

## Checklist
- [ ] Package name is snake_case
- [ ] package.xml has correct `<depend>` entries
- [ ] setup.py / CMakeLists.txt installs launch/ and config/ to share/
- [ ] At least one placeholder test file created in test/
