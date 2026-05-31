cmake_minimum_required(VERSION 3.8)
project(robot_msgs)

find_package(ament_cmake REQUIRED)

# rosidl_generate_interfaces omitted until msg/srv/action files are added

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)  # copyright deferred — source files lack headers
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
