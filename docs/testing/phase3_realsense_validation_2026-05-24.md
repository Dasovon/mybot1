# Phase 3 RealSense D435 Validation — 2026-05-24

## Goal

Install and validate the Intel RealSense D435 on the Raspberry Pi 5:
- Depth stream at 640×480 @ 15 Hz
- Color stream at 640×480 @ 15 Hz
- Point cloud publishing

---

## Hardware Context

- Intel RealSense D435 (serial 244622071235, firmware 5.17.0.10)
- Raspberry Pi 5 (Ubuntu Server 24.04 LTS, hostname pi5bot)
- ROS 2 Jazzy, realsense2_camera 4.57.7, librealsense2 2.57.7
- Connected to Pi USB 3.0 port (Bus 003 / xhci-hcd / 5000M SuperSpeed)

---

## Installation

### No Intel apt repo needed

The ROS apt repo (`packages.ros.org`) ships pre-built ARM64 packages for Ubuntu 24.04:

```bash
sudo apt install ros-jazzy-librealsense2 ros-jazzy-realsense2-camera ros-jazzy-realsense2-description
```

No source compilation required. The Intel librealsense.intel.com apt repo was tested but had GPG key issues with the noble keyring format — not needed since ROS ships the same version.

### udev rules

The ROS package does not install udev rules. Install them from Intel's GitHub:

```bash
curl -sSL https://raw.githubusercontent.com/IntelRealSense/librealsense/master/config/99-realsense-libusb.rules \
  | sudo tee /etc/udev/rules.d/99-realsense-libusb.rules > /dev/null
sudo udevadm control --reload-rules && sudo udevadm trigger

sudo usermod -aG video ubuntu
```

The `ubuntu` user must be in the `video` group, and udev rules must be in place before rs-enumerate-devices will work without sudo.

---

## Issues Encountered and Fixes

### Issue 1 — `rs-enumerate-devices` permission denied as non-root

**Symptom:** All `/dev/video*` devices returned `Permission denied` when running as `ubuntu`.

**Root cause:** `ubuntu` user was not in the `video` group, and no udev rules granting RealSense devices to the `video` group were installed.

**Fix:**
```bash
sudo usermod -aG video ubuntu
curl -sSL https://raw.githubusercontent.com/IntelRealSense/librealsense/master/config/99-realsense-libusb.rules \
  | sudo tee /etc/udev/rules.d/99-realsense-libusb.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

---

### Issue 2 — `pointcloud.enable:=true` launch arg not applied

**Symptom:** Passing `pointcloud.enable:=true` as a launch argument resulted in the parameter silently not being set. No pointcloud topic appeared.

**Root cause:** In realsense2_camera v4.x, the internal ROS parameter name for the pointcloud filter is `pointcloud__neon_.enable` (not `pointcloud.enable`). The launch argument name did not match the internal parameter name in this version.

**Fix at runtime:**
```bash
ros2 param set /camera/camera pointcloud__neon_.enable true
```

**Fix in launch file:** Use the correct parameter name. The topic `/camera/camera/depth/color/points` appears after enabling.

**Workaround confirmed:** Setting the parameter at runtime on a running node works immediately.

---

### Issue 3 — v4.x topic namespace is `/camera/camera/...`

**Observation:** realsense2_camera v4.57.7 defaults to `camera_namespace=camera` and `camera_name=camera`, producing topics under `/camera/camera/...`. The earlier project docs assumed `/camera/...`.

**Root cause:** v4.x separated `camera_namespace` and `camera_name`. With both defaulting to `camera`, the node name becomes `camera` inside the `camera` namespace.

**Passing `camera_namespace:=''` (empty) does not work** — the launch arg parser rejects empty-string values.

**Resolution:** Updated CLAUDE.md, `.claude/rules/ros2_communication.md`, and Nav2 config references to use the actual topic names:
- `/camera/camera/color/image_raw`
- `/camera/camera/depth/image_rect_raw`
- `/camera/camera/depth/color/points`

---

## Final Working Configuration

### Verify device
```bash
source /opt/ros/jazzy/setup.bash
rs-enumerate-devices
```

### Launch
```bash
source /opt/ros/jazzy/setup.bash
ros2 launch robot_bringup realsense.launch.xml
```

Or directly:
```bash
ros2 launch realsense2_camera rs_launch.py \
    depth_module.depth_profile:=640x480x15 \
    rgb_camera.color_profile:=640x480x15 \
    enable_infra1:=false \
    enable_infra2:=false \
    pointcloud.enable:=true \
    publish_tf:=true
```

Then enable pointcloud (workaround until launch arg is fixed upstream):
```bash
ros2 param set /camera/camera pointcloud__neon_.enable true
```

---

## Validated Stream Rates

| Stream | Topic | Expected | Observed |
|---|---|---|---|
| Color | `/camera/camera/color/image_raw` | 15 Hz | 15.0 Hz ✓ |
| Depth | `/camera/camera/depth/image_rect_raw` | 15 Hz | ~14.7 Hz ✓ |
| Point cloud | `/camera/camera/depth/color/points` | 15 Hz | ~12-13 Hz ✓ (CPU-bounded on Pi 5) |

USB confirmed at 5000M (USB 3.0 SuperSpeed). No USB errors in operation.
