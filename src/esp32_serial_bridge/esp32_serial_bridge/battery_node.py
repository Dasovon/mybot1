import threading
import time

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy
from sensor_msgs.msg import BatteryState

from ina219 import INA219

BAT_FULL_V   = 12.6   # 3S LiPo: 4.2 V/cell
BAT_CUTOFF_V = 9.9    # 3S LiPo: 3.3 V/cell
SHUNT_OHMS   = 0.1    # on-board R100 shunt
I2C_BUS      = 1      # Pi I2C-1 (GPIO 2/3)
PUBLISH_HZ   = 1.0


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

        self._pub = self.create_publisher(BatteryState, '/battery_state', qos)
        self._reader = INA219Reader()
        self.create_timer(1.0 / PUBLISH_HZ, self._publish)
        self.get_logger().info('battery_publisher started — INA219 on Pi I2C-1 (0x40)')

    def _publish(self):
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
            self.get_logger().warn('INA219 read failed — publishing stale battery state',
                                   throttle_duration_sec=10.0)

        self._pub.publish(msg)


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
