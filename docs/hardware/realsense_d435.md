# Intel RealSense D435 — RGB-D Depth Camera

## Role in This Project

The RealSense D435 provides RGB-D (color + depth) data used for:
- 3D obstacle detection via the Nav2 voxel costmap layer
- Validation of LiDAR-detected geometry (distinguish walls from clutter)
- Future: visual odometry, RTAB-Map, YOLO semantic perception

---

## Key Specs

| Property | Value |
|---|---|
| Depth technology | Active IR stereo |
| Depth range | 0.1–10 m (ideal: 0.3–3 m) |
| Depth resolution | Up to 1280×720 |
| Depth frame rate | Up to 90 FPS |
| RGB resolution | 1920×1080 |
| RGB frame rate | Up to 30 FPS |
| Field of view (depth) | 87° × 58° × 95° (H×V×D) |
| Interface | USB 3.1 Gen 1 (USB-C, adapter to USB-A) |
| Power | Bus-powered via USB |

---

## Wiring — Confirmed Connections

```
RealSense D435
    └── USB 3.0 cable (USB-C → USB-A)
            └── Raspberry Pi USB 3.0 port (blue)
```

| Property | Value |
|---|---|
| Interface | USB 3.0 (required — USB 2.0 is insufficient bandwidth) |
| Connected to | Raspberry Pi 5 USB 3.0 port |
| Power | Bus-powered via USB |
| Driver backend | RSUSB (confirmed working on Pi — no kernel module required) |
| Operating config | 640×480 @ 15 FPS depth + color |

> Must use a USB 3.0 port (blue). Connecting to USB 2.0 causes reduced frame rates or silent failure.

---

## ROS 2 Driver

Package: `realsense2_camera`

```bash
sudo apt install ros-humble-realsense2-camera
```

Key published topics:

| Topic | Type | Use |
|---|---|---|
| `/camera/depth/image_rect_raw` | `sensor_msgs/Image` | Raw depth image |
| `/camera/depth/points` | `sensor_msgs/PointCloud2` | 3D point cloud |
| `/camera/color/image_raw` | `sensor_msgs/Image` | RGB image |
| `/camera/aligned_depth_to_color/image_raw` | `sensor_msgs/Image` | Depth aligned to RGB |

Launch example:

```bash
ros2 launch realsense2_camera rs_launch.py \
  enable_pointcloud:=true \
  pointcloud_texture_stream:=RS2_STREAM_COLOR
```

---

## TF Frames

```
base_link → camera_link → camera_depth_frame
                        → camera_color_frame
                        → camera_left_ir_frame
                        → camera_right_ir_frame
```

Define `camera_link` in the URDF relative to `base_link`. The internal camera sub-frames are published automatically by the RealSense driver.

---

## Nav2 Voxel Layer

The point cloud from `/camera/depth/points` feeds the Nav2 voxel costmap layer for 3D obstacle detection. Configure in `nav2_params.yaml`:

```yaml
voxel_layer:
  enabled: true
  publish_voxel_map: true
  origin_z: 0.0
  z_resolution: 0.05
  z_voxels: 16
  observation_sources: pointcloud
  pointcloud:
    topic: /camera/depth/points
    min_obstacle_height: 0.05
    max_obstacle_height: 2.0
    obstacle_max_range: 3.0
    data_type: PointCloud2
```

---

## Performance Notes

- On Raspberry Pi 5, limit depth resolution to 640×480 at 30 FPS for stable USB 3.0 throughput.
- Point cloud processing (voxel filtering, passthrough) should run on the **development PC**, not the Pi.
- Use `depth_registered` streams when aligning depth to color for semantic fusion.
- Keep USB cable short (< 1 m) to avoid bandwidth degradation.

---

## Future Use (RTAB-Map / YOLO)

When upgrading to RTAB-Map, the D435 RGB-D stream becomes the primary sensor for:
- Visual loop closure
- RGB-D SLAM
- Semantic segmentation via YOLO on the development PC GPU
