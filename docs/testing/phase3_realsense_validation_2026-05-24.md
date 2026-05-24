# Phase 3 RealSense D435 Validation — 2026-05-24

## Goal

Install and validate the Intel RealSense D435 on the Raspberry Pi 5:
- Depth stream at 424×240 @ 6 Hz
- Color stream at 424×240 @ 15 Hz
- RGB8-colored point cloud publishing in RViz2

---

## Hardware Context

- Intel RealSense D435 (serial 244622071235, firmware 5.17.0.10)
- Raspberry Pi 5 (Ubuntu Server 24.04 LTS, hostname pi5bot)
- Dev PC: Ubuntu 24.04, ROS 2 Jazzy (ros-jazzy-desktop)
- realsense2_camera 4.57.7, librealsense2 2.57.7
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

**Root cause:** In realsense2_camera v4.x, the internal ROS parameter name for the pointcloud filter is `pointcloud__neon_.enable` (not `pointcloud.enable`). The launch argument name does not match the internal parameter name.

**Workaround (runtime):**
```bash
ros2 param set /camera/camera pointcloud__neon_.enable true
ros2 param set /camera/camera pointcloud__neon_.stream_filter 2
```

**Fix via config_file:** Pass a flat YAML to rs_launch.py's `config_file` arg using the `__neon_` runtime names directly (see Final Configuration below).

---

### Issue 3 — v4.x topic namespace is `/camera/camera/...`

**Observation:** realsense2_camera v4.57.7 defaults to `camera_namespace=camera` and `camera_name=camera`, producing topics under `/camera/camera/...`. The earlier project docs assumed `/camera/...`.

**Root cause:** v4.x separated `camera_namespace` and `camera_name`. With both defaulting to `camera`, the node name becomes `camera` inside the `camera` namespace. Passing `camera_namespace:=''` is rejected.

**Resolution:** Updated CLAUDE.md and `.claude/rules/ros2_communication.md` to use the actual topic names:
- `/camera/camera/color/image_raw`
- `/camera/camera/depth/image_rect_raw`
- `/camera/camera/depth/color/points`

---

### Issue 4 — No RGB8 option in RViz2 PointCloud2 Color Transformer

**Symptom:** PointCloud2 Color Transformer showed only Intensity, Flat Color, Axis Color — no RGB8.

**Root cause:** `pointcloud__neon_.stream_filter` was 0 (ANY stream). With filter=0, no color data is projected into the point cloud — the PointCloud2 messages have no `rgb` field.

**Fix:** Set `stream_filter` to 2 (COLOR stream). Confirmed by checking message fields:
```bash
ros2 topic echo --once /camera/camera/depth/color/points | grep -A 30 'fields:'
# name: rgb  ← appears after fix
```

**Baked into config:** `pointcloud__neon_.stream_filter: 2` in `config/realsense.yaml`.

---

### Issue 5 — WiFi bandwidth saturation at 640×480 × 15 Hz

**Symptom:** RViz2 froze when displaying depth image + color image + point cloud simultaneously.

**Fix:** Reduced depth to 424×240 @ 6 Hz. Color stays at 424×240 @ 15 Hz (color data embedded in point cloud, so separate image stream is not displayed in RViz2). Removed image display panels from RViz2 — DDS stops transmitting unsubscribed topics.

---

### Issue 6 — Point cloud orientation wrong in RViz2 (scene "pointing up")

**Symptom:** Point cloud appeared vertical relative to the grid floor.

**Root cause:** Fixed Frame was set to `camera_color_optical_frame`, which uses optical convention (Z=forward/depth, Y=down). RViz2's grid is in the XY plane with Z=up, so the depth axis appeared vertical.

**Fix:** Set RViz2 Fixed Frame to `camera_link` (robot convention: X=forward, Z=up). The camera node publishes the `camera_link → camera_color_optical_frame` TF, so RViz2 can transform the data correctly.

---

### Issue 7 — Point cloud colors poorly aligned (fringing)

**Symptom:** Color pixels in the point cloud did not align with geometry — visible color fringing on edges.

**Root cause:** `align_depth.enable: false` — depth was not being reprojected into the color camera frame before texturing.

**Fix:** `align_depth.enable: true`. Color quality improved significantly.

---

## Final Working Configuration

### Config file: `src/robot_bringup/config/realsense.yaml`

Flat format for rs_launch.py `config_file` arg (NOT ROS2 namespaced YAML):

```yaml
rgb_camera.color_profile: 424x240x15
depth_module.depth_profile: 424x240x6
pointcloud__neon_.enable: true
pointcloud__neon_.stream_filter: 2      # COLOR → rgb field in PointCloud2
align_depth.enable: true                # aligns color to depth for clean texturing
decimation_filter.enable: false
```

### Launch on Pi

```bash
~/launch_realsense.sh
```

Which runs:
```bash
source /opt/ros/jazzy/setup.bash
ros2 launch realsense2_camera rs_launch.py \
    config_file:=$HOME/realsense_config/realsense.yaml \
    publish_tf:=true
```

Then apply pointcloud workaround (until config_file `__neon_` names are confirmed to apply at startup):
```bash
source /opt/ros/jazzy/setup.bash
ros2 param set /camera/camera pointcloud__neon_.enable true
ros2 param set /camera/camera pointcloud__neon_.stream_filter 2
```

### View in RViz2 (dev PC)

```bash
source /opt/ros/jazzy/setup.bash
rviz2
```

- Add PointCloud2 display → topic `/camera/camera/depth/color/points`
- Fixed Frame: `camera_link`
- Color Transformer: **RGB8**
- Size (m): `0.003`–`0.005`

---

## Validated Stream Rates

| Stream | Topic | Rate |
|---|---|---|
| Color | `/camera/camera/color/image_raw` | 15.0 Hz ✓ |
| Depth | `/camera/camera/depth/image_rect_raw` | 6.0 Hz ✓ |
| Point cloud (RGB8) | `/camera/camera/depth/color/points` | ~6 Hz ✓ |

USB confirmed at 5000M (USB 3.0 SuperSpeed). No USB errors in operation.

**RViz2:** RGB8 colored point cloud confirmed rendering. Color well-aligned to geometry with `align_depth.enable: true`.
