"""Unit tests for ShutdownTimer one-shot deadline logic."""
from esp32_serial_bridge.shutdown_timer import ShutdownTimer
import pytest


def test_below_cutoff_before_deadline_returns_false():
    timer = ShutdownTimer(shutdown_after_s=30.0)
    assert timer.update(in_cutoff=True, elapsed_s=29.9) is False


def test_reaching_deadline_returns_true_once():
    timer = ShutdownTimer(shutdown_after_s=30.0)
    assert timer.update(in_cutoff=True, elapsed_s=30.0) is True


def test_no_repeated_request_after_deadline():
    timer = ShutdownTimer(shutdown_after_s=30.0)
    timer.update(in_cutoff=True, elapsed_s=30.0)
    assert timer.update(in_cutoff=True, elapsed_s=31.0) is False
    assert timer.update(in_cutoff=True, elapsed_s=60.0) is False


def test_recovery_then_new_cutoff_permits_new_shutdown():
    timer = ShutdownTimer(shutdown_after_s=30.0)
    timer.update(in_cutoff=True, elapsed_s=30.0)
    timer.update(in_cutoff=False, elapsed_s=0.0)
    assert timer.update(in_cutoff=True, elapsed_s=30.0) is True


def test_not_in_cutoff_returns_false():
    timer = ShutdownTimer(shutdown_after_s=30.0)
    assert timer.update(in_cutoff=False, elapsed_s=999.0) is False


def test_recovery_clears_shutdown_requested_flag():
    timer = ShutdownTimer(shutdown_after_s=30.0)
    timer.update(in_cutoff=True, elapsed_s=30.0)
    assert timer.shutdown_requested is True
    timer.update(in_cutoff=False, elapsed_s=0.0)
    assert timer.shutdown_requested is False


def test_zero_shutdown_after_s_raises():
    with pytest.raises(ValueError):
        ShutdownTimer(shutdown_after_s=0.0)


def test_negative_shutdown_after_s_raises():
    with pytest.raises(ValueError):
        ShutdownTimer(shutdown_after_s=-1.0)


def test_negative_elapsed_raises():
    timer = ShutdownTimer(shutdown_after_s=30.0)
    with pytest.raises(ValueError):
        timer.update(in_cutoff=True, elapsed_s=-1.0)
