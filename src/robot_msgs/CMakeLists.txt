cmake_minimum_required(VERSION 3.8)
project(robot_msgs)

find_package(ament_cmake REQUIRED)
find_package(rosidl_default_generators REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  # Add .msg, .srv, .action files here as they are created
  # "msg/EnvironmentalStatus.msg"
  # "msg/RobotTelemetry.msg"
)

ament_package()
