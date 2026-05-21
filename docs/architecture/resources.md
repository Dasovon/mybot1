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

**Status:** Early-stage / pre-release — limited public documentation as of May 2026. Watch the repo: https://github.com/agenticros/agenticros-skills

**Four deployment modes:**

| Mode | Transport | Use case |
|------|-----------|----------|
| A — Same machine | DDS (native) | Agent runs on the Pi directly |
| B — Local network | WebSocket → rosbridge_server | **Best fit for mybot1** — Claude Code on dev PC talks to Pi over LAN |
| C — Cloud/remote | WebRTC + STUN/TURN | Fleet / remote ops behind NAT |
| D — Zenoh | zenoh-ts WebSocket → Zenoh router | Low-latency, no rosbridge dependency |

**Mode B for mybot1:**
```
Claude Code (dev PC 192.168.86.52)
  → AgenticROS plugin
      → WebSocket → rosbridge_server on Pi (192.168.86.33)
          → /diff_cont/cmd_vel_unstamped
          → /battery_state  (INA219 data)
          → Nav2 NavigateToPose action
          → /camera/color/image_raw
```
Requires `rosbridge_server` running on the Pi: `ros2 launch rosbridge_server rosbridge_websocket_launch.xml`

**Skills architecture:**
- Plugin system: skills register as tools via `registerSkill(api, config, context)`
- Config lives under `plugins.entries.agenticros.config.skillPackages` — tune without code changes
- Reference skill: `agenticros-skill-followme` (depth-based person tracking with optional VLM)
- Skills publish to `cmd_vel` and query camera feeds — same interfaces mybot1 already exposes

**mybot1 topic mapping:**
- `/cmd_vel` in AgenticROS docs → `/diff_cont/cmd_vel_unstamped` in mybot1
- `/battery_state` matches directly — INA219 data already published at 1 Hz
- Camera: `/camera/color/image_raw` (RealSense D435)

**Target phase: Phase 5+.** Requires rosbridge on Pi and Nav2 running. Mode B is zero-config once rosbridge is installed.

---

### Tier 2 — Useful Background / Future Work

#### Claude Code as Embodied Agent (Brian Tsui / FAEA)
https://medium.com/@brianytsui/claude-code-as-embodied-agent-to-control-robots-85-96-success-in-sim-with-zero-demonstration-455a9044d353

Great research reading but less immediately applicable — it uses privileged simulator state rather than real sensor data, is explicitly simulation-only with no real-world transfer demonstrated, and precision manipulation fails entirely. However, the ReAct loop methodology (reason → write code → execute → observe → iterate) is exactly how you'd want to use Claude Code when debugging your SLAM behaviors or tuning Nav2 parameters.

**What it actually is:** FAEA (Frontier Agent for Embodied Actions) — Claude Code + Claude Opus as the model, writing Python against a robotics simulator API. Hobby project, nights and weekends.

**Results (simulation only):**

| Benchmark | Success Rate | Tasks |
|-----------|--------------|-------|
| LIBERO | 84.9% (88.2% with coaching) | 120 |
| ManiSkill3 | 85.7% | 14 |
| MetaWorld | 96.0% (100% with coaching) | 50 |

Zero demonstrations. 2–26 attempts per task, 2–25 minutes, 18,473 tool calls across 419 tasks.

**Why it doesn't transfer to mybot1 (or any real robot):**
- Inputs are privileged simulator state (exact object positions) — no perception, no real sensors
- Each decision cycle takes 2–8 seconds — incompatible with any ROS2 control loop
- Peg insertion and plug charging: 0% success — sub-millimeter precision is out of reach
- No sim-to-real transfer demonstrated; authors explicitly flag it as future work
- Cost: $0.51–$5.60 per task — not viable at scale

**What is applicable to mybot1:**
The ReAct loop is the right mental model for using Claude Code as a debugging partner on real hardware:
```
Reason → write a diagnostic script / config change
Execute → ros2 topic echo, colcon build, launch
Observe → check topic rates, TF tree, costmap output
Iterate → adjust PID gains, EKF config, Nav2 params, repeat
```
This is exactly how to work through SLAM drift, Nav2 costmap tuning, or encoder calibration — not as an autonomous agent, but as an interactive loop where you observe real sensor output between steps.

#### Claude Code Ultimate Guide
https://github.com/FlorianBruniaux/claude-code-ultimate-guide

24K+ lines across 16 areas — the most comprehensive Claude Code reference available. Not robotics-specific but foundational for using Claude Code well on a complex multi-package project.

**Most relevant sections for mybot1:**
- **Agent teams**: Parallel worktree agents for cross-package refactoring — useful when reorganizing the ROS2 package structure or migrating node architectures across multiple packages simultaneously
- **MCP vetting (5-phase workflow)**: Provenance check → code review → permission whitelist → Docker sandbox → ongoing monitoring. Apply this before adding any MCP server that touches robot hardware or sensor data
- **Dangerous-actions hook**: 37 production hooks available; use the dangerous-actions variant to block `rm -rf`, force-push, and uncontrolled hardware commands in Claude sessions
- **"Artifact paradox"**: Claude Code can generate 1.75× more logic errors than human code (ACM 2025). For robotics: mandatory review gates before any hardware deployment, not just CI

**7-layer config map** (useful mental model for this project):

| Layer | Use in mybot1 |
|-------|--------------|
| Commands (`.claude/commands/`) | `/build`, `/test`, `/ros2` — already set up |
| Skills | Calibration routines, sensor validation sequences |
| Agents (`.claude/agents/`) | `ros2-reviewer`, `hardware-checker` — already set up |
| Hooks | Post-edit colcon build, pre-deploy safety checks |
| MCP servers | Future: rosbag analysis, Nav2 sim interface |
| CLAUDE.md | Already set up with full hardware constants and standards |
| Memory | Auto-saved preferences per session |

#### Claude Code Power User Tips
https://support.claude.com/en/articles/14554000-claude-code-power-user-tips

Official Anthropic tips. Several are directly useful for mybot1 development:

**High-value for this project:**
- **`Shift+Tab`** — cycles through modes: default → acceptEdits → plan → auto. Use plan mode before any multi-file refactor across ROS2 packages
- **`/batch`** — fans work to parallel worktree agents. Useful when updating hardware constants across all packages at once (e.g., if a topic name changes)
- **`/permissions`** — pre-approve safe commands (`colcon build`, `ros2 topic hz`) to reduce prompts during debugging sessions
- **`/loop`** — schedule recurring local tasks (up to 3 days). Could automate periodic `colcon test` runs
- **Worktrees**: `claude --worktree my_feature` — isolated session per feature branch. Essential when testing firmware changes alongside ROS2 changes without cross-contamination
- **CLAUDE.md compounding**: after each correction, append *"Update your CLAUDE.md so you don't make that mistake again."* Already being done; this is the right habit
- **Verification principle**: *"Giving Claude a way to verify its work will markedly improve quality."* For ROS2: always append `&& colcon build && colcon test` to implementation requests

**For multi-package work:**
```bash
claude --add-dir ~/bot_ws/src/esp32_serial_bridge --add-dir ~/bot_ws/src/robot_bringup
```
Add `"additionalDirectories"` to `settings.json` to make this persistent across sessions.

#### Claude Code Robotics Engineer Agent
https://github.com/rohitg00/awesome-claude-code-toolkit/blob/main/agents/specialized-domains/robotics-engineer.md

A robotics-specific agent persona spec. **Cannot be dropped in as-is** — it's a prose specification, not a Claude agent config file. Needs conversion before use.

**What the persona enforces (worth borrowing):**
- Separate nodes for sensor drivers, perception, state estimation, planning, and control — matches mybot1's package structure
- tf2 tree must be a consistent tree with no loops, every frame has exactly one parent
- RELIABLE QoS for commands, BEST_EFFORT for high-frequency sensor data — already in `.claude/rules/ros2_communication.md`
- Control loops in dedicated threads with no dynamic memory allocation or blocking I/O
- "A missed deadline is not performance degradation — it is a potential collision" — the right mindset for PID loop and watchdog code

**Verdict:** The constraints are already encoded in `.claude/rules/robot_specific.md` and `hardware_constants.md`. No need to add this agent; the rules files cover it with mybot1-specific values rather than generic patterns.

---

### Tier 3 — Reference / Background

#### ROS2 Engineering Skills
https://github.com/dbwls99706/ros2-engineering-skills

More substantial than its URL suggests — **20 production-grade reference modules, 13,000+ lines, 429 unit tests** verified on Humble/Jazzy/Rolling. Worth pulling into `.claude/skills/` when the project reaches Phase 4+.

**Most relevant modules for mybot1:**
- `navigation.md` — Nav2 costmap tuning, slam_toolbox, collision avoidance
- `hardware-interface.md` — Motor controllers, encoder feedback via ros2_control
- `communication.md` — QoS RxO semantics, silent message drop diagnosis (mismatched profiles)
- `launch-system.md` — Composable bringup: hardware + TF broadcaster + SLAM + Nav2 in one launch
- `testing.md` — launch_testing to verify sensor data flows before field trials

**Included tooling:**
- `qos_checker.py` — validates pub/sub QoS compatibility, catches silent drops before runtime
- `launch_validator.py` — static AST analysis of Python launch files
- `create_package.py` — scaffolds C++/Python packages with lifecycle patterns and gtest harness

#### ROS 2 for Software Engineers
https://medium.com/codex/explaining-ros-2-to-software-engineers-ecd7afdcc1d8

Intro-level article framing ROS2 concepts via software engineering analogies: nodes as microservices, packages as modules, topics as pub-sub, services as synchronous RPC, actions as async long-running ops. No value if you already know ROS2. Useful to share with contributors who have a strong software background but no robotics experience.

#### Skill marketplaces (low signal)
The following were checked — most return 403 or contain only generic ROS2 boilerplate with no mybot1-specific value:
- https://mcpmarket.com/tools/skills/ros-2-service-pattern
- https://mcpmarket.com/tools/skills/ros-2-launch-system-manager
- https://www.claudemarketplace.net/skills/ros2-launch-system
- https://smithery.ai/skills/zeeshan080/ros2-patterns (403)
- https://skills.rest/skill/ros2-patterns
- https://skills.rest/skill/ros2-skill
- https://clawhub.ai/dbwls99706/ros2-engineering-skills
- https://lobehub.com/nl/skills/lpigeon-ros-skill

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
│  │  Reads /dev/ttyACM0 (ESP32 Serial0 display telemetry)         │   │
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
│  │    /diff_cont/odom  ← encoder counts     │  ┌────────────────┐  │
│  │    /imu/imu        ← BNO055             │  │ display_       │  │
│  │    /battery_state  ← INA219             │  │ telemetry      │  │
│  │  Subscribes:                             │  │ (Serial0)      │  │
│  │    /diff_cont/cmd_vel_unstamped → motors │  │ JSON @ 2Hz     │  │
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
│  RP Lidar ──USB──► /dev/rplidar  →  rplidar_ros2  →  /scan        │
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
| Wheel odometry | Encoders → ESP32-S3 | micro-ROS Serial1 → `/dev/ttyUSB0` | `/diff_cont/odom` → robot_localization EKF |
| IMU | BNO055 → ESP32-S3 | micro-ROS Serial1 → `/dev/ttyUSB0` | `/imu/imu` → robot_localization EKF |
| Battery | INA219 → ESP32-S3 | micro-ROS Serial1 → `/dev/ttyUSB0` | `/battery_state` → monitoring |
| Battery (display) | INA219 → ESP32-S3 | JSON Serial0 → `/dev/ttyACM0` | display_daemon → OLED |
| Laser scan | RP Lidar | USB → `/dev/rplidar` | `/scan` → slam_toolbox |
| Depth / color | RealSense D435 (RSUSB) | USB 3.0 | `/camera/*` → Nav2 costmap |
| Motor commands | Nav2 | ROS2 topic | `/diff_cont/cmd_vel_unstamped` → micro-ROS → TB6612 → motors |
| Map | slam_toolbox | ROS2 topic | `/map` → Nav2 |
| System stats | Pi 5 (psutil) | local | display_daemon → OLED |

### Key Architectural Decisions

**ESP32-S3 has two serial roles:** `Serial1` (physical UART pins) carries microROS to the Pi on one USB port; `Serial0` (USB CDC) streams INA219 JSON telemetry to the Pi on a second USB port. This keeps display data completely decoupled from ROS2 — the OLED keeps updating even if ROS2 is down.

**Display daemon is ROS2-blind by design:** It's a plain Python systemd service that starts at boot regardless of ROS2 state. Battery and system info are always visible, even during bring-up, crashes, or reflashing.

**RealSense uses RSUSB backend:** No kernel module needed. Runs entirely in userspace, which matters on Pi 5 with its newer kernel. The depth stream feeds both Nav2's 3D costmap (as a pointcloud) and the upcoming OpenCV object tracking pipeline.

**SLAM → Nav2 pipeline:** slam_toolbox runs in mapping mode first to build the map, then switches to localization mode for autonomous navigation. The RP Lidar `/scan` is the primary localization input; the RealSense pointcloud adds 3D obstacle awareness to the costmap.

---

## What You Get (Display System)

**`display_telemetry.ino` (ESP32-S3)** — Reads INA219 every 500ms over I2C (SDA=GPIO 8, SCL=GPIO 9) and streams compact JSON over USB CDC serial (Serial0 → `/dev/ttyACM0`): `{"v":12.34,"i":1.23,"p":15.16,"ok":1,"ts":12345}`. Uses the Adafruit INA219 library. Calibrated for 32V/2A by default — adjust in the sketch if your draw exceeds 2A.

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
| BNO055 SDA | GPIO 8 | 0x28 | shared I2C bus with INA219 and BME680 |
| BNO055 SCL | GPIO 9 | | |
| INA219 SDA | GPIO 8 | 0x40 | shared I2C bus |
| INA219 SCL | GPIO 9 | | |
| BME680 SDA | GPIO 8 | 0x76 | not yet wired — Phase 6 |
| BME680 SCL | GPIO 9 | | |
| BNO055 VCC | 3.3V | | |
| INA219 VCC | 3.3V | | |
| BNO055 GND | GND | | |
| INA219 GND | GND | | |

#### ESP32-S3 → TB6612

| ESP32-S3 | TB6612 | Notes |
|----------|--------|-------|
| GPIO 10 (LEDC ch 0) | PWMA | Right motor speed, 20 kHz 8-bit PWM |
| GPIO 11 | AIN1 | Right motor direction A |
| GPIO 12 | AIN2 | Right motor direction B |
| GPIO 13 (LEDC ch 1) | PWMB | Left motor speed, 20 kHz 8-bit PWM |
| GPIO 14 | BIN1 | Left motor direction A |
| GPIO 15 | BIN2 | Left motor direction B |
| — | STBY | Not wired — Adafruit board has 10 kΩ pull-up (always HIGH) |
| 3.3V | VCC | Logic power |
| GND | GND | |

**Motor A = RIGHT, Motor B = LEFT.** Do not swap.

⚠️ VM carries battery voltage (12V+). Keep VM wiring physically separate from all signal pins.

#### TB6612 → Motors

| TB6612 | Motor |
|--------|-------|
| AO1, AO2 | Right motor (Motor A = RIGHT) |
| BO1, BO2 | Left motor (Motor B = LEFT) |

#### Motor Encoders → ESP32-S3

| Encoder | ESP32-S3 Pin | Notes |
|---------|-------------|-------|
| Left enc A | GPIO 40 | `attachInterrupt` CHANGE ⚠️ EMI — 100 nF cap to GND required |
| Left enc B | GPIO 41 | read in ISR ⚠️ EMI — 100 nF cap to GND required |
| Right enc A | GPIO 42 | `attachInterrupt` CHANGE |
| Right enc B | GPIO 39 | read in ISR |
| VCC (×2) | 3.3V | JGA25-371 encoder power (Blue/Black wires) |
| GND (×2) | GND | |

Wire colors (JGA25-371): Red/White = motor power, Blue/Black = encoder power, Yellow = Ch A, Green = Ch B.

⚠️ GPIO 40/41 (left encoder) picks up TB6612 20 kHz PWM switching noise. Add 100 nF ceramic caps from GPIO 40 → GND and GPIO 41 → GND in the signal path. EMA filter (VEL_ALPHA = 0.2) in firmware also required (or use PCNT hardware encoder — see build_plan.md).

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
| RP Lidar | USB-A port 3 | `/dev/rplidar` (udev symlink) |
| RealSense D435 | USB-A port 4 | RSUSB backend — use a USB 3.0 port (blue) |

### Key Improvements Over MyBot

**EP-0225 for Pi power** — clean PD power management, auto-boot on battery connect, proper soft shutdown via long-press. Eliminates Pi brownout risk from motor noise on a shared rail.

**INA219 on main trunk** — whole-robot current visibility on the OLED and in ROS2, not just partial draw.

**Two dedicated USB connections from ESP32-S3** — microROS and display telemetry separated from the start, no retrofitting needed.

**RealSense on USB 3.0** — Pi 5 has two USB 3.0 ports (blue). Assign the D435 to one intentionally.

**Anti-spark connector** — protects battery connectors from arcing on every power-up.

**1000µF cap on TB6612 VM** — absorbs motor switching noise before it hits the rest of the system.

### Hardware Decisions — Status

| Decision | Status | Value |
|----------|--------|-------|
| ESP32-S3 GPIO for TB6612 | ✅ Confirmed | GPIO 10–15 (see wiring table above) |
| ESP32-S3 GPIO for encoders | ✅ Confirmed | GPIO 39–42 (see wiring table above) |
| I2C pins | ✅ Confirmed | GPIO 8 (SDA), GPIO 9 (SCL) |
| micro-ROS serial | ✅ Confirmed | Serial1, GPIO 17 TX / 18 RX, `/dev/ttyUSB0` |
| Display telemetry serial | ✅ Confirmed | Serial0, USB CDC, `/dev/ttyACM0` |
| Encoder CPR | ✅ Validated on floor | 1010 counts/rev |
| Wheel radius | ✅ Measured | 0.034 m |
| Wheel separation | ✅ Measured | 0.179 m |
| LiPo connector type | ⏳ Undecided | XT30 or XT60 — standardize across both robots |
| Blade fuse on motor rail | ⏳ Optional | Between INA219 VIN- and TB6612 VM — cheap insurance |
| Boost converter on Pi rail | ⏳ Optional | MT3608 set to 15V — eliminates battery sag risk under load |
