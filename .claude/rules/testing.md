# Testing Rules — mybot1

## Test file locations

| Test type | Location | Framework |
|---|---|---|
| Unit tests (Python) | `src/<package>/test/test_<module>.py` | pytest |
| Unit tests (C++) | `src/<package>/test/<test_name>.cpp` | GTest |
| Integration / launch tests | `src/<package>/test/test_<feature>_launch.py` | pytest + launch_testing |
| Validation protocols (human) | `docs/testing/<feature>_test_protocol_<YYYY-MM-DD>.md` | checklist |
| Validation run logs | `docs/testing/<feature>_validation_<YYYY-MM-DD>.md` | recorded results |

Never put test scripts in `docs/testing/` — docs only.

## Python unit test pattern

```python
# test/test_navigation_service.py
import pytest
from <package>.application.navigation_service import NavigationService
from <package>.domain.interfaces import MotorController
from <package>.domain.robot_state import RobotState
from unittest.mock import MagicMock

@pytest.fixture
def mock_controller():
    ctrl = MagicMock(spec=MotorController)
    ctrl.get_state.return_value = RobotState(0, 0, 0, 0, 0, 12.4)
    return ctrl

def test_cmd_vel_sets_wheel_velocities(mock_controller):
    svc = NavigationService(mock_controller)
    svc.set_cmd_vel(0.5, 0.0)
    svc.step()
    mock_controller.set_velocity.assert_called_once()
    left, right = mock_controller.set_velocity.call_args[0]
    assert abs(left - 0.5) < 1e-6
    assert abs(right - 0.5) < 1e-6
```

## C++ GTest pattern

```cpp
// test/test_pid_controller.cpp
#include <gtest/gtest.h>
#include "<package>/domain/pid_controller.hpp"

TEST(PIDController, ZeroErrorProducesZeroOutput) {
    PIDController pid(1.0, 0.0, 0.0);
    EXPECT_NEAR(pid.compute(0.0, 0.0, 0.01), 0.0, 1e-6);
}

TEST(PIDController, ProportionalGainApplied) {
    PIDController pid(2.0, 0.0, 0.0);
    double output = pid.compute(1.0, 0.0, 0.01);
    EXPECT_NEAR(output, 2.0, 1e-6);
}

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

## Launch / integration test pattern

```python
# test/test_robot_launch.py
import unittest
import launch
import launch_ros.actions
import launch_testing
import launch_testing.actions
import pytest

@pytest.mark.launch_test
def generate_test_description():
    node = launch_ros.actions.Node(
        package='robot_bringup',
        executable='<node_name>',
    )
    return launch.LaunchDescription([
        node,
        launch_testing.actions.ReadyToTest(),
    ]), {'node': node}

class TestNodeStartup(unittest.TestCase):
    def test_node_starts(self, proc_output):
        proc_output.assertWaitFor('started', timeout=10)
```

## Run tests

```bash
# All tests in workspace
cd ~/bot_ws && colcon test --packages-select <package_name>
colcon test-result --verbose

# Single test file
cd ~/bot_ws && python3 -m pytest src/<package>/test/test_<module>.py -v
```

## Hardware validation protocol docs

Every phase gate in `docs/architecture/build_plan.md` must have a corresponding protocol in `docs/testing/`. File naming:
- `<phase>_test_protocol_<YYYY-MM-DD>.md` for the checklist
- `<phase>_validation_<YYYY-MM-DD>.md` for the recorded run result

---

## Motor test safety — hard rules (updated 2026-05-30)

**The robot has proven strong enough to damage itself. These rules are absolute.**

### Normal approved motion path — use the test script

```bash
# On Pi — handles bag, drive, stop, and cleanup atomically:
~/motor_test.sh [vx_m_s] [duration_s]
~/motor_test.sh 0.10 8
```

This is the required path for all motor tests. It starts the bag before motion and stops it after. Do not assemble manual bag + publisher combos.

### Manual cmd_vel forms (script internals / emergency only)

```bash
# Drive (foreground — SSH blocks until complete):
ros2 topic pub --times 160 --rate 20 /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist "{linear: {x: 0.10}}"

# Stop (single shot):
ros2 topic pub --once /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist "{linear: {x: 0.0}, angular: {z: 0.0}}"
```

### Emergency stop
```bash
for i in {1..20}; do ros2 topic pub --once /diff_cont/cmd_vel_unstamped geometry_msgs/msg/Twist "{linear: {x: 0.0}, angular: {z: 0.0}}"; sleep 0.05; done
```

**Never use `run_in_background: true` on a cmd_vel publisher.** A background publisher keeps running after a queued stop finishes. Robot hit a wall 2026-05-30 from this pattern.

### Watchdog status

Current firmware runtime value: `WATCHDOG_MS = 2000`. The intended safety target for motor testing is 500 ms. This is an open safety mismatch — do not assume the robot stops within 500 ms after command loss until firmware is updated to 500 ms, flashed, and stop-time validated.

---

## Data capture rules — mandatory for all motor tests

**Every motor test must be recorded.** Use `motor_test.sh` — it starts the bag before the drive and stops it after. Do not analyze from memory or from a single `echo --once` snapshot.

### Topics recorded by motor_test.sh

`/diff_cont/cmd_vel_unstamped` is included so commanded vs. measured velocity can be compared in the same bag.

### Why each topic is required

| Topic | Rate | What to look for |
|---|---|---|
| `/diff_cont/odom` | 30 Hz | Encoder-derived wheel velocity. Compare `twist.linear.x` and `twist.angular.z` vs. commanded. This is the primary PID tracking signal. |
| `/imu/imu` | 30 Hz | `angular_velocity.z` reveals yaw rate during a commanded straight drive — should be near zero. `linear_acceleration.x` reveals jerk: spikes indicate abrupt starts/stops or oscillation. Use this to detect PID overshoot and mechanical chatter. |
| `/battery_state` | 1 Hz | `voltage` and `current` under motor load. Current spikes indicate stall or aggressive PID output. Voltage sag under load flags power supply issues. |
| `/diff_cont/cmd_vel_unstamped` | 20 Hz | Records exactly what was commanded. Confirms the publisher ran at the intended rate and lets you overlay command vs. response. |

### Analysis requirements

After every motor test, report all of the following before changing any parameter:

1. **Velocity tracking** — plot or tabulate `cmd_vel.linear.x` vs `odom.twist.linear.x` over time. Compute steady-state error as a percentage of commanded velocity.
2. **Yaw drift** — report `odom.twist.angular.z` (encoder) mean and peak. This is the authoritative yaw signal. `imu.angular_velocity.z` is also recorded but is vibration-contaminated under motor load (validated: mean +0.113 rad/s, spikes to ±11.3 rad/s during straight drive); report it as a diagnostic metric only, not as evidence of wheel imbalance.
3. **Jerk / oscillation** — report `imu.linear_acceleration.x` peak-to-peak as a diagnostic. Note: acceleration spikes are also contaminated by gearbox vibration and are not a reliable PID oscillation indicator until the IMU is mechanically isolated. Do not use IMU jerk as the basis for PID tuning decisions.
4. **Current draw** — report `battery_state.current` mean and peak. A peak above 2× steady-state current suggests stall or windup.
5. **Distance accuracy** — report `odom.pose.pose.position.x` at end of run vs. expected (`cmd_vel * duration`).

### Bag playback on dev PC

```bash
# List bag contents
ros2 bag info ~/test_logs/<bag_name>

# Play back (Pi or dev PC)
ros2 bag play ~/test_logs/<bag_name>

# Extract to CSV for analysis (dev PC)
ros2 bag convert -i ~/test_logs/<bag_name> -o /tmp/bag_csv --output-format csv
```

### Rule summary

- **No motor test without a bag running.** If you forget to start the bag, re-run the test.
- **No parameter change without completing the full 5-point analysis above.** Single-metric reasoning (e.g. "position moved, looks good") is not sufficient.
- **All sensor streams are always recorded, even if only one is under investigation.** IMU, encoder, and INA219 interact — a problem that looks like a PID issue may be an EMI issue visible only in IMU jerk, or a power issue visible only in current draw.

---

## After-test stop rule

After every motor test or data capture, **stop immediately and present the data to the user before doing anything else.**

Do not:
- Proceed to analysis without pausing
- Queue a follow-up command
- Change a parameter
- Re-run the test
- Diagnose and propose a fix in the same turn as collecting data

Do:
1. Present the raw results (bag info, topic counts, key numbers)
2. State clearly what the data shows and what problem it reveals
3. State what you propose to do next and why
4. **Wait for the user to confirm direction before acting**

This applies even if the result is obviously wrong or the bag is corrupt. Stop, report, wait.

**Why:** The user needs to see the data independently before any interpretation is applied. A diagnosis that looks obvious from one metric may be wrong when the user sees the full picture. Acting on incomplete or misread data wastes a test run and can introduce bad parameter changes.
