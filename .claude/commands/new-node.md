# /new-node

Create a new ROS 2 node in an existing package.

## Usage
`/new-node <package_name> <node_name> [--lifecycle]`

## Steps

1. Read `docs/architecture/build_plan.md` to confirm the node fits the current phase
2. Determine the package: Python (`ament_python`) or C++ (`ament_cmake`)
3. Create the node file following the architecture pattern in `.claude/rules/ros2_nodes.md`:
   - Logic class in `src/<package>/<package>/` (no ROS imports)
   - Node class in `src/<package>/<package>/<node_name>_node.py` (ROS wiring only)
4. Add entry point to `src/<package>/setup.py` (Python) or `CMakeLists.txt` (C++)
5. Create a launch file in `src/<package>/launch/<node_name>.launch.py`
6. Create a YAML param file in `src/<package>/config/<node_name>.yaml`
7. Use QoS profiles from `.claude/rules/ros2_communication.md` — never default QoS for sensor or control topics
8. If `--lifecycle`, use `LifecycleNode` pattern

## Checklist before finishing
- [ ] Node declares all parameters with defaults
- [ ] QoS profiles match the topic type (SENSOR/CONTROL/STATE)
- [ ] Logic is in a separate class, not in the Node
- [ ] Entry point added to setup.py / CMakeLists.txt
- [ ] Node logs name and key params at startup (`self.get_logger().info(...)`)
- [ ] Hardware constants use values from `.claude/rules/hardware_constants.md`
