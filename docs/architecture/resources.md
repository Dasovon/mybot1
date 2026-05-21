# MyBot — Resources & System Architecture

---

## Most Relevant Resources for Your Robot

### Tier 1 — Directly Actionable

#### Henki ROS 2 Best Practices
https://henkirobotics.com/ros-2-best-practices/

This is your single best find. It covers node design with single-responsibility principles, separating ROS 2 communication logic from core application logic, proper use of actions vs services, QoS settings in parameter files, composable nodes for performance, and executor choices — and uniquely demonstrates integrating these best practices directly into Claude Code via `CLAUDE.md`. For your architecture — ESP32-S3 feeding sensor data up to the Pi over microROS, diff drive control, SLAM on top — this tells you exactly how to structure your packages cleanly. The before/after example with a speed monitor node is essentially analogous to your encoder/odometry pipeline.

#### ROS2 + Claude Code Template
https://github.com/harunkurtdev/ros2-claude-code-template

This establishes a Clean Architecture environment for ROS2 with a `.claude/rules` directory for architectural standards and a `.claude/skills` directory for implementation templates covering node creation, lifecycle management, messaging patterns, TF2 transforms, launch configuration, and bag recording. This is the scaffolding you'd want in `mybot_ws` when using Claude Code to help develop your packages. The rules directory is the key piece — it tells Claude Code how to write ROS2 code correctly for your project.

#### Controlling ROS2 with Claude via MCP
https://medium.com/@itsrarjun/controlling-a-ros2-robot-with-claude-using-mcp-31732c9b1616

This covers exposing ROS2 capabilities as MCP tools — topics like `/cmd_vel`, actions, services, and sensor data — so Claude can send commands in natural language that map directly to ROS2 pub/sub and action servers. Once your ESP32-S3 microROS bridge is running and topics are flowing, this pattern lets you drive, inspect, and debug the robot by talking to it. Particularly useful for your Nav2 + SLAM setup once mapping is working.

**Target phase: Phase 5+ (Nav2 running).** Use earlier (Phase 1–3) for manual drive testing and sensor inspection before Nav2 is up.

**Architecture for mybot1:**
```
Claude (MCP client)
  → MCP server (Python + rclpy, sourced into ROS2 env on Pi or dev PC)
      → publishes to /diff_cont/cmd_vel_unstamped  (CONTROL_QOS / RELIABLE)
      → subscribes to /diff_cont/odom, /scan, /battery_state
      → calls Nav2 NavigateToPose action server
```

**Useful tools to expose:**
- `drive(linear_m_s, angular_rad_s)` → publishes `geometry_msgs/Twist` to `/diff_cont/cmd_vel_unstamped`
- `stop()` → publishes zero-velocity Twist
- `get_battery()` → reads latest `/battery_state` message (INA219 data)
- `get_scan_hz()` → wraps `ros2 topic hz /scan` — confirms LiDAR is live
- `navigate_to(x, y, theta)` → sends `NavigateToPose` action goal to Nav2
- `get_odom()` → reads latest `/diff_cont/odom` for position/velocity

**mybot1-specific implementation notes:**
- The MCP server process must `source ~/bot_ws/install/setup.bash` before importing `rclpy`
- `/cmd_vel` here is `/diff_cont/cmd_vel_unstamped` — use CONTROL_QOS (RELIABLE/VOLATILE), not default QoS
- Nav2 goal tool needs the `NavigateToPose` action client, not just a topic publisher
- Camera tool reads `/camera/color/image_raw` with SENSOR_QOS (BEST_EFFORT)
- The article is conceptual only — no code provided. Write the MCP server from scratch using `rclpy` directly

#### AgenticROS
https://discourse.openrobotics.org/t/agenticros-connects-ros-with-openclaw-claude-code-desktop-dispatch-and-google-gemini/53699

AgenticROS supports four deployment modes: same machine (local DDS), local network (WebSocket via rosbridge), cloud/remote (WebRTC with NAT traversal), and Zenoh. It supports Nav2 goals, `/cmd_vel` velocity commands, camera frame capture, and battery/sensor queries. The local network mode is a natural fit — Claude Code on your dev machine at `192.168.86.52` talking to MyBot at `192.168.86.33`. The battery state query via `/battery_state` maps directly to your INA219 data. Worth watching closely.

---

### Tier 2 — Useful Background / Future Work

#### Claude Code as Embodied Agent (Brian Tsui / FAEA)
https://medium.com/@brianytsui/claude-code-as-embodied-agent-to-control-robots-85-96-success-in-sim-with-zero-demonstration-455a9044d353

Great research reading but less immediately applicable — it uses privileged simulator state rather than real sensor data, is explicitly simulation-only with no real-world transfer demonstrated, and precision manipulation fails entirely. However, the ReAct loop methodology (reason → write code → execute → observe → iterate) is exactly how you'd want to use Claude Code when debugging your SLAM behaviors or tuning Nav2 parameters.

#### Claude Code Ultimate Guide
https://github.com/FlorianBruniaux/claude-code-ultimate-guide

Covers CLAUDE.md setup, agent teams, MCP server integration, security hardening, and agentic workflows — practical for setting up Claude Code effectively on your dev machine for the MyBot workspace. Most directly useful for getting Claude Code configured well for your ROS2 development workflow rather than anything robotics-specific, but foundational.

#### Claude Code Power User Tips
https://support.claude.com/en/articles/14554000-claude-code-power-user-tips

Official Anthropic tips for getting the most out of Claude Code — useful alongside the Ultimate Guide when setting up your dev workflow.

#### Claude Code Robotics Engineer Agent
https://github.com/rohitg00/awesome-claude-code-toolkit/blob/main/agents/specialized-domains/robotics-engineer.md

A pre-built Claude Code agent persona tuned for robotics engineering. Drop it into your `.claude/agents/` directory in `mybot_ws` to give Claude Code a robotics-aware default mindset when working on your packages.

---

### Tier 3 — Less Relevant to Your Immediate Goals

The following are useful reference material for ROS2 patterns and skills in general, but not specific to diff drive SLAM with your hardware stack. Worth bookmarking for when you're deep in package architecture or need Claude Code skill templates:

- https://mcpmarket.com/tools/skills/ros-2-service-pattern
- https://mcpmarket.com/tools/skills/ros-2-launch-system-manager
- https://www.claudemarketplace.net/skills/ros2-launch-system
- https://smithery.ai/skills/zeeshan080/ros2-patterns
- https://skills.rest/skill/ros2-patterns
- https://skills.rest/skill/ros2-skill
- https://github.com/dbwls99706/ros2-engineering-skills
- https://clawhub.ai/dbwls99706/ros2-engineering-skills
- https://lobehub.com/nl/skills/lpigeon-ros-skill
- https://medium.com/codex/explaining-ros-2-to-software-engineers-ecd7afdcc1d8

---

## Full System Architecture (MyBot)

Repos:
- MyBot (active dev, fully wired): https://github.com/Dasovon/MyBot
- MyBot1 (fresh build): https://github.com/Dasovon/mybot1

```
┌─────────────────────────────────────────────────────────────────────┐
│                        RASPBERRY PI 5                               │
│                       ryan@mybot / ~/mybot_ws                       │
│                                                                     │
│  ┌─────────────────────────── ROS2 ──────────────────────────────┐  │
│  │                                                               │  │
│  │  ┌──────────────┐    /odom        ┌──────────────────────┐   │  │
│  │  │ micro_ros_   │◄───────────────►│   robot_state_       │   │  │
│  │  │ agent        │    /cmd_vel     │   publisher /        │   │  │
│  │  │ (USB serial) │    /imu         │   diff_drive_        │   │  │
│  │  │              │    /battery     │   controller         │   │  │
│  │  └──────────────┘                └──────────────────────┘   │  │
│  │         │ USB                            │                   │  │
│  │         │                         /tf  /tf_static            │  │
│  │  ┌──────────────┐                        │                   │  │
│  │  │  rplidar_    │──/scan─────────►┌──────────────────────┐   │  │
│  │  │  ros2        │                 │   slam_toolbox       │   │  │
│  │  └──────────────┘                 │   (mapping/loc)      │   │  │
│  │         │ USB                     └──────────────────────┘   │  │
│  │  ┌──────────────┐                        │ /map              │  │
│  │  │  realsense2_ │──/camera/──────►┌──────────────────────┐   │  │
│  │  │  camera      │  depth/color    │   Nav2 stack         │   │  │
│  │  │  (RSUSB)     │──/camera/──────►│   (planner,          │   │  │
│  │  └──────────────┘  pointcloud     │    controller,       │   │  │
│  │         │ USB                     │    costmaps)         │   │  │
│  │         │                         └──────────────────────┘   │  │
│  │  (future: OpenCV object tracking)         │ /cmd_vel          │  │
│  │                                           └──────────────────┘  │
│  └───────────────────────────────────────────────────────────────┘  │
│                                                                     │
│  ┌──────────────────── DISPLAY DAEMON ──────────────────────────┐   │
│  │  display_daemon.py  (systemd, ROS2-independent)              │   │
│  │  Reads /dev/ttyUSB1 (ESP32 telemetry)                        │   │
│  │  Reads psutil (CPU/RAM/disk/temp)                            │   │
│  │  Writes SPI → SSD1309 OLED @ 2Hz                             │   │
│  └──────────────────────────────────────────────────────────────┘   │
│           │ SPI (BCM10/11/8/25/27)                │ USB             │
└───────────┼───────────────────────────────────────┼─────────────────┘
            │                                       │
            ▼                                       │ (separate USB port)
   ┌─────────────────┐                              │
   │  2.42" OLED     │                              │
   │  SSD1309 128×64 │                              │
   │  Waveshare      │                              │
   └─────────────────┘                              │
                                                    │
┌───────────────────────────────────────────────────┼─────────────────┐
│                     ESP32-S3 (DevKitC)            │                 │
│                  feature/esp32-microros            │                 │
│                                                   │ USB CDC         │
│  ┌──────────────────────────────────────────┐    │ Serial0         │
│  │  microROS (Serial1 / UART)               │────┘ (telemetry)    │
│  │  Publishes:                              │                      │
│  │    /odom       ← encoder counts          │  ┌────────────────┐  │
│  │    /imu        ← BNO055                  │  │ display_       │  │
│  │    /battery    ← INA219                  │  │ telemetry      │  │
│  │  Subscribes:                             │  │ (Serial0)      │  │
│  │    /cmd_vel    → TB6612 → motors         │  │ JSON @ 2Hz     │  │
│  └──────────────────────────────────────────┘  └────────────────┘  │
│                                                                     │
│  ┌──────────┐  I2C   ┌──────────┐                                  │
│  │ BNO055   │────────│ INA219   │                                  │
│  │ IMU      │        │ power    │                                  │
│  │ 0x28     │        │ 0x40     │                                  │
│  └──────────┘        └──────────┘                                  │
│                                                                     │
│  UART/PWM ──► TB6612 motor driver ──► JGA25-371 motors (×2)        │
│                                         45:1 gearbox               │
│                                         1010 enc counts/rev        │
│                                         ┌─────────────────────┐    │
│                                         │ wheel_sep = 0.179m  │    │
│                                         │ wheel_r   = 0.034m  │    │
│                                         └─────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                      SENSORS (USB to Pi)                            │
│                                                                     │
│  RP Lidar ──USB──► /dev/ttyUSB0  →  rplidar_ros2  →  /scan        │
│  RealSense D435 ──USB──► realsense2_camera (RSUSB) →  /camera/*   │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                    DEV MACHINE                                      │
│                 ryan@dev / 192.168.86.52 / ~/dev_ws                 │
│                                                                     │
│  Claude Code ──WiFi──► MyBot (192.168.86.33)                        │
│  RViz2  /  rqt  /  ssh                                              │
│  AgenticROS (Mode B: local network via rosbridge)                   │
└─────────────────────────────────────────────────────────────────────┘
```

### Data Flow Summary

| Data | Source | Transport | Sink |
|------|--------|-----------|------|
| Wheel odometry | Encoders → ESP32-S3 | microROS USB serial | `/odom` → diff_drive_controller → Nav2 |
| IMU | BNO055 → ESP32-S3 | microROS USB serial | `/imu` → robot_localization / Nav2 |
| Battery | INA219 → ESP32-S3 | microROS USB serial | `/battery` → monitoring |
| Battery (display) | INA219 → ESP32-S3 | JSON serial (separate USB port) | display_daemon → OLED |
| Laser scan | RP Lidar | USB | `/scan` → slam_toolbox |
| Depth / color | RealSense D435 (RSUSB) | USB | `/camera/*` → Nav2 costmap / OpenCV |
| Motor commands | Nav2 | ROS2 topic | `/cmd_vel` → microROS → TB6612 → motors |
| Map | slam_toolbox | ROS2 topic | `/map` → Nav2 |
| System stats | Pi 5 (psutil) | local | display_daemon → OLED |

### Key Architectural Decisions

**ESP32-S3 has two serial roles:** `Serial1` (physical UART pins) carries microROS to the Pi on one USB port; `Serial0` (USB CDC) streams INA219 JSON telemetry to the Pi on a second USB port. This keeps display data completely decoupled from ROS2 — the OLED keeps updating even if ROS2 is down.

**Display daemon is ROS2-blind by design:** It's a plain Python systemd service that starts at boot regardless of ROS2 state. Battery and system info are always visible, even during bring-up, crashes, or reflashing.

**RealSense uses RSUSB backend:** No kernel module needed. Runs entirely in userspace, which matters on Pi 5 with its newer kernel. The depth stream feeds both Nav2's 3D costmap (as a pointcloud) and the upcoming OpenCV object tracking pipeline.

**SLAM → Nav2 pipeline:** slam_toolbox runs in mapping mode first to build the map, then switches to localization mode for autonomous navigation. The RP Lidar `/scan` is the primary localization input; the RealSense pointcloud adds 3D obstacle awareness to the costmap.

---

## What You Get (Display System)

**`display_telemetry.ino` (ESP32-S3)** — Reads INA219 every 500ms over I2C (SDA=IO17, SCL=IO16) and streams compact JSON over USB CDC serial: `{"v":12.34,"i":1.23,"p":15.16,"ok":1,"ts":12345}`. Uses the Adafruit INA219 library. Calibrated for 32V/2A by default — adjust in the sketch if your draw exceeds 2A.

**`display_daemon.py` (Pi)** — Standalone Python service. Reads the serial stream with auto-detection of the ESP32's USB port and automatic reconnection on disconnect. Collects CPU/RAM/disk/temp via psutil. Renders to the SSD1309 via luma.oled over SPI at 2Hz. Battery percentage is estimated from voltage (defaults to 3S LiPo 9.6V–12.6V range — edit to match your pack).

**`mybot-display.service`** — systemd unit that starts at boot, restarts automatically on failure, runs as user `ryan` with `dialout` group for serial access. Completely independent of any ROS2 launch.

**Key coexistence note:** Since the ESP32-S3 also runs microROS, the cleanest split is telemetry on `Serial0` (USB CDC) while microROS uses `Serial1` via physical UART pins — keeping them on separate Pi USB ports.

---

## MyBot1 — Fresh Build

Repo: https://github.com/Dasovon/mybot1

MyBot1 is a clean-slate rebuild using MyBot as a reference. Blank SD card, not yet wired. Same core hardware stack, but wired intentionally from the start with lessons learned from MyBot. The EP-0225 PD power board replaces ad-hoc Pi power and adds proper auto-boot and soft shutdown.

**EP-0225 reference:** https://wiki.52pi.com/index.php?title=EP-0225

### Power Architecture

```
3S LiPo (~11.1V nominal, 12.6V full, 9.6V cutoff)
       │
  [XT60 anti-spark connector]
       │
  [MAIN SWITCH]
       │
  [INA219 VIN+]──[SHUNT]──[INA219 VIN-]──────────────────────┐
                                          │                   │
                                   EP-0225 DC barrel    TB6612 VM
                                   (9–24V in)           (motor rail)
                                   3.5mm/1.35mm         + 1000µF cap
                                   center positive      across VM/GND
                                          │                   │
                                   USB-C → Pi 5        JGA25-371
                                   (5V/8A, 40W)        motors (×2)
                                   Always-ON: ON

3S LiPo (−) ──────────────────── GND bus (common ground)
```

The INA219 sits on the **main trunk after the switch, before the rail split** — measures total robot current draw (Pi + motors combined).

### Power Design Decisions & Analysis

#### Main Switch on LiPo Positive ✅ Good
Single switch before everything — simple and reliable. Add an **XT60 anti-spark connector** on the battery side. 3S LiPo into the capacitors on the TB6612 and EP-0225 will arc on connect without it, degrading connectors over time.

#### TB6612 Direct from 3S LiPo ✅ Good, with one addition
TB6612 is rated for VM up to 15V so 12.6V fully charged 3S is safe. Add a **1000µF capacitor across VM and GND on the TB6612** — motor direction changes and braking inject voltage spikes back onto the rail. This cap absorbs them before they reach the EP-0225 or ESP32. Cheap and standard practice.

#### EP-0225 from 3S LiPo DC Barrel ✅ Good, with a caveat
11.1V nominal sits comfortably in the EP-0225's 9–24V input range. However a 3S LiPo at low charge hits ~9.6V at rest — under motor load it can sag below 9V and brown out the Pi. Two mitigations:

- **Option A (recommended) — LiPo low-voltage alarm:** Never run the pack below 10V under load. Simple, no extra hardware in the power path.
- **Option B — Boost converter:** MT3608-based boost between battery and EP-0225, set to 15V. Eliminates sag risk entirely, improves Pi power quality. ~$3, small footprint. Worth it if running the battery hard or deep into discharge.

#### EP-0225 Always-ON Switch: SET TO ON
Robot boots automatically when battery is connected. No need to press the power button on every startup. Long-press (3s) for soft shutdown when powered on.

#### INA219 on Main Trunk ✅ Improvement over MyBot
Measures whole-robot current (Pi + motors), not just partial. Full picture on the OLED and in ROS2.

### Full Wiring Table

#### Power Rail

| From | To | Wire | Notes |
|------|----|------|-------|
| LiPo (+) | XT60 anti-spark | 18AWG red | |
| XT60 out | Main switch IN | 18AWG red | |
| Main switch OUT | INA219 VIN+ | 18AWG red | |
| INA219 VIN- | EP-0225 DC barrel (+) | 18AWG red | 3.5mm/1.35mm, center positive |
| INA219 VIN- | TB6612 VM | 18AWG red | |
| TB6612 VM | 1000µF cap (+) | — | cap (−) to GND bus, mount at TB6612 |
| LiPo (−) | GND bus | 18AWG black | common ground for everything |
| EP-0225 DC barrel (−) | GND bus | 18AWG black | |
| TB6612 GND | GND bus | 18AWG black | |

#### EP-0225 → Pi 5

| EP-0225 | Pi 5 | Notes |
|---------|------|-------|
| USB-C out | USB-C power in | 5V/8A, 40W max |
| Always-ON switch | — | Set to ON position |

#### ESP32-S3 → Pi 5 (USB)

| ESP32-S3 | Pi 5 USB port | Purpose |
|----------|--------------|---------|
| USB (Serial1 / microROS) | USB-A port 1 | microROS transport |
| USB (Serial0 / telemetry) | USB-A port 2 | INA219 JSON → display daemon |

#### ESP32-S3 → Sensors (I2C)

| Device | ESP32-S3 Pin | I2C Address | Notes |
|--------|-------------|-------------|-------|
| BNO055 SDA | IO17 | 0x28 | same I2C bus as INA219 |
| BNO055 SCL | IO16 | | |
| INA219 SDA | IO17 | 0x40 | same I2C bus as BNO055 |
| INA219 SCL | IO16 | | |
| BNO055 VCC | 3.3V | | |
| INA219 VCC | 3.3V | | |
| BNO055 GND | GND | | |
| INA219 GND | GND | | |

#### ESP32-S3 → TB6612

| ESP32-S3 | TB6612 | Notes |
|----------|--------|-------|
| PWM pin A | PWMA | Motor A speed |
| PWM pin B | PWMB | Motor B speed |
| AIN1 pin | AIN1 | Motor A direction |
| AIN2 pin | AIN2 | Motor A direction |
| BIN1 pin | BIN1 | Motor B direction |
| BIN2 pin | BIN2 | Motor B direction |
| STBY pin | STBY | Pull HIGH to enable |
| 3.3V | VCC | Logic power |
| GND | GND | |

> **Note:** Specific GPIO pin numbers TBD — assign intentionally based on physical layout before wiring. Do not inherit MyBot's pin assignments.

#### TB6612 → Motors

| TB6612 | Motor |
|--------|-------|
| AO1, AO2 | Left motor |
| BO1, BO2 | Right motor |

#### Motor Encoders → ESP32-S3

| Encoder | ESP32-S3 Pin | Notes |
|---------|-------------|-------|
| Left enc A | TBD GPIO | interrupt-capable pin |
| Left enc B | TBD GPIO | interrupt-capable pin |
| Right enc A | TBD GPIO | interrupt-capable pin |
| Right enc B | TBD GPIO | interrupt-capable pin |
| VCC (×2) | 3.3V | verify encoder voltage rating |
| GND (×2) | GND | |

#### Pi 5 → OLED Display (SPI)

| OLED | Pi 5 Board Pin | BCM |
|------|---------------|-----|
| VCC | Pin 1 | 3.3V |
| GND | Pin 6 | GND |
| DIN | Pin 19 | BCM10 (MOSI) |
| CLK | Pin 23 | BCM11 (SCLK) |
| CS | Pin 24 | BCM8 (CE0) |
| DC | Pin 22 | BCM25 |
| RST | Pin 13 | BCM27 |

#### Pi 5 → Sensors (USB)

| Device | Pi 5 Port | Notes |
|--------|----------|-------|
| RP Lidar | USB-A port 3 | `/dev/ttyUSB0` |
| RealSense D435 | USB-A port 4 | RSUSB backend — use a USB 3.0 port (blue) |

### Key Improvements Over MyBot

**EP-0225 for Pi power** — clean PD power management, auto-boot on battery connect, proper soft shutdown via long-press. Eliminates Pi brownout risk from motor noise on a shared rail.

**INA219 on main trunk** — whole-robot current visibility on the OLED and in ROS2, not just partial draw.

**Two dedicated USB connections from ESP32-S3** — microROS and display telemetry separated from the start, no retrofitting needed.

**RealSense on USB 3.0** — Pi 5 has two USB 3.0 ports (blue). Assign the D435 to one intentionally.

**Anti-spark connector** — protects battery connectors from arcing on every power-up.

**1000µF cap on TB6612 VM** — absorbs motor switching noise before it hits the rest of the system.

### Things Still To Decide

- **Specific ESP32-S3 GPIO assignments** for TB6612 and encoders — lock these in based on physical board layout before soldering
- **LiPo connector type** — XT30 or XT60, standardize across both robots
- **Optional blade fuse** on motor rail between INA219 VIN- and TB6612 VM — cheap insurance
- **Optional boost converter** on Pi power rail if battery sag under load is a concern
