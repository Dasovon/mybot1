import os
import subprocess
import threading
import time

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy
from sensor_msgs.msg import BatteryState
from geometry_msgs.msg import Twist

from ina219 import INA219

BAT_FULL_V    = 12.6   # 3S LiPo: 4.2 V/cell
BAT_CUTOFF_V  = 9.9    # 3S LiPo: 3.3 V/cell — enter cutoff below this
BAT_RECOVER_V = 10.2   # hysteresis: exit cutoff above this (0.3 V band)
SHUNT_OHMS    = 0.1    # on-board R100 shunt
I2C_BUS       = 1      # Pi I2C-1 (GPIO 2/3)
PUBLISH_HZ    = 1.0
CUTOFF_HZ     = 40.0   # zero-cmd_vel publish rate during cutoff
SHUTDOWN_AFTER_S = 30  # seconds below cutoff before OS shutdown to protect battery


# ---------------------------------------------------------------------------
# NOTE — cutoff strategy (Option 1, band-aid):
# During low-voltage cutoff this node races Nav2/teleop by publishing zero
# Twist at CUTOFF_HZ (40 Hz). It wins in practice but has no true priority.
# TODO: once Nav2 is active, replace with twist_mux so the cutoff topic has
# guaranteed priority over navigation commands instead of relying on timing.
# See: https://github.com/ros-navigation/twist_mux
# ---------------------------------------------------------------------------


class INA219Reader:
    """Reads INA219 at 1 Hz in a background thread. Thread-safe voltage/current."""

    def __init__(self):
        self.voltage = 0.0   # V
        self.current = 0.0   # A
        self.fresh = False
        self._lock = threading.Lock()
        self._ina = None
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        while True:
            try:
                if self._ina is None:
                    self._ina = INA219(SHUNT_OHMS, busnum=I2C_BUS)
                    self._ina.configure()
                v = self._ina.voltage()
                i = self._ina.current() / 1000.0  # mA → A
                with self._lock:
                    self.voltage = v
                    self.current = i
                    self.fresh = True
            except Exception as e:
                self._ina = None
                with self._lock:
                    self.fresh = False
            time.sleep(1.0 / PUBLISH_HZ)

    def read(self):
        with self._lock:
            return self.voltage, self.current, self.fresh


class BatteryPublisherNode(Node):
    def __init__(self):
        super().__init__('battery_publisher')

        qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
        )
        cmd_qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
        )

        self._pub = self.create_publisher(BatteryState, '/battery_state', qos)
        self._cmd_pub = self.create_publisher(Twist, '/diff_cont/cmd_vel_unstamped', cmd_qos)
        self._reader = INA219Reader()

        self._in_cutoff = False
        self._cutoff_since = None  # monotonic time when cutoff began

        self.create_timer(1.0 / PUBLISH_HZ, self._publish_battery)
        self.create_timer(1.0 / CUTOFF_HZ, self._cutoff_tick)

        self.get_logger().info(
            f'battery_publisher started — INA219 on Pi I2C-1 (0x40), '
            f'cutoff={BAT_CUTOFF_V}V recover={BAT_RECOVER_V}V')

    def _publish_battery(self):
        voltage, current, fresh = self._reader.read()

        msg = BatteryState()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.voltage = float(voltage)
        msg.current = float(current)
        msg.power_supply_status = BatteryState.POWER_SUPPLY_STATUS_DISCHARGING
        msg.present = fresh

        if fresh:
            pct = (voltage - BAT_CUTOFF_V) / (BAT_FULL_V - BAT_CUTOFF_V)
            msg.percentage = max(0.0, min(1.0, pct))
        else:
            self.get_logger().warn(
                'INA219 read failed — publishing stale battery state',
                throttle_duration_sec=10.0)

        self._pub.publish(msg)
        self._update_cutoff_state(voltage, fresh)

    def _update_cutoff_state(self, voltage: float, fresh: bool):
        if not fresh or voltage <= 0.0:
            return

        if not self._in_cutoff and voltage < BAT_CUTOFF_V:
            self._in_cutoff = True
            self._cutoff_since = time.monotonic()
            self.get_logger().error(
                f'BATTERY CRITICAL: {voltage:.2f} V < {BAT_CUTOFF_V} V — '
                f'publishing zero cmd_vel at {CUTOFF_HZ:.0f} Hz. '
                f'Shutdown in {SHUTDOWN_AFTER_S}s if voltage does not recover.')

        elif self._in_cutoff and voltage >= BAT_RECOVER_V:
            self._in_cutoff = False
            self._cutoff_since = None
            self.get_logger().warn(
                f'Battery recovered to {voltage:.2f} V — resuming normal operation.')

        if self._in_cutoff:
            elapsed = time.monotonic() - self._cutoff_since
            self.get_logger().error(
                f'Battery cutoff active: {voltage:.2f} V  ({elapsed:.0f}s)',
                throttle_duration_sec=5.0)

            if elapsed >= SHUTDOWN_AFTER_S:
                self.get_logger().error(
                    f'Battery below cutoff for {SHUTDOWN_AFTER_S}s — initiating shutdown.')
                subprocess.Popen(['sudo', 'shutdown', '-h', 'now'])

    def _cutoff_tick(self):
        """Publishes zero velocity at CUTOFF_HZ while in cutoff state.
        Runs every tick regardless; no-op when not in cutoff — cheap timer."""
        if not self._in_cutoff:
            return
        self._cmd_pub.publish(Twist())


def main(args=None):
    rclpy.init(args=args)
    node = BatteryPublisherNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()
