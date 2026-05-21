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
