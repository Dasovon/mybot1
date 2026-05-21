# /test

Run tests for a package or the whole workspace.

## Usage
`/test [package_name]`

## Commands

Run all tests:
```bash
cd ~/bot_ws && colcon test && colcon test-result --verbose
```

Run tests for one package:
```bash
cd ~/bot_ws && colcon test --packages-select <package_name>
colcon test-result --all --verbose
```

Run a specific test file:
```bash
cd ~/bot_ws && python3 -m pytest src/<package>/test/test_<module>.py -v
```

Run with coverage:
```bash
cd ~/bot_ws && python3 -m pytest src/<package>/test/ --cov=<package> --cov-report=term
```

## Test locations
- Unit tests: `src/<package>/test/test_<module>.py`
- Launch tests: `src/<package>/test/test_<feature>_launch.py`
- See `.claude/rules/testing.md` for patterns

## Hardware validation
For hardware-in-loop validation, follow the protocol in `docs/testing/`. Do not write pass/fail results back to a protocol file — create a separate validation log.
