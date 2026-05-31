"""Unit tests for BatteryGuard cutoff state machine."""
from esp32_serial_bridge.battery_guard import BatteryGuard
import pytest


def test_enters_cutoff_below_threshold():
    guard = BatteryGuard(cutoff_voltage=9.9, recovery_voltage=10.2)
    result = guard.update(9.8, fresh=True)
    assert result.in_cutoff is True
    assert result.entered_cutoff is True


def test_does_not_enter_cutoff_at_threshold():
    guard = BatteryGuard(cutoff_voltage=9.9, recovery_voltage=10.2)
    result = guard.update(9.9, fresh=True)
    assert result.in_cutoff is False


def test_does_not_enter_cutoff_above_threshold():
    guard = BatteryGuard(cutoff_voltage=9.9, recovery_voltage=10.2)
    result = guard.update(11.5, fresh=True)
    assert result.in_cutoff is False
    assert result.entered_cutoff is False


def test_hysteresis_requires_recovery_voltage():
    guard = BatteryGuard(cutoff_voltage=9.9, recovery_voltage=10.2)
    guard.update(9.8, fresh=True)
    still_cutoff = guard.update(10.1, fresh=True)
    recovered = guard.update(10.2, fresh=True)
    assert still_cutoff.in_cutoff is True
    assert recovered.in_cutoff is False
    assert recovered.recovered is True


def test_stale_read_does_not_change_normal_state():
    guard = BatteryGuard(cutoff_voltage=9.9, recovery_voltage=10.2)
    result = guard.update(0.0, fresh=False)
    assert result.in_cutoff is False
    assert result.entered_cutoff is False


def test_stale_read_does_not_clear_cutoff():
    guard = BatteryGuard(cutoff_voltage=9.9, recovery_voltage=10.2)
    guard.update(9.8, fresh=True)
    result = guard.update(0.0, fresh=False)
    assert result.in_cutoff is True
    assert result.recovered is False


def test_recovery_flag_only_set_once():
    guard = BatteryGuard(cutoff_voltage=9.9, recovery_voltage=10.2)
    guard.update(9.8, fresh=True)
    guard.update(10.2, fresh=True)
    result = guard.update(10.5, fresh=True)
    assert result.recovered is False


def test_invalid_threshold_configuration_rejected():
    with pytest.raises(ValueError):
        BatteryGuard(cutoff_voltage=10.2, recovery_voltage=9.9)


def test_equal_thresholds_rejected():
    with pytest.raises(ValueError):
        BatteryGuard(cutoff_voltage=9.9, recovery_voltage=9.9)
