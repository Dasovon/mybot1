#!/usr/bin/env python3
"""
Holds /dev/rplidar open with DTR=False to keep the RPLidar A1 motor stopped.

Hardware finding (2026-05-31): on this build, dtr=False in pyserial stops the
motor; dtr=True runs it. The CP2102 adapter reverts to motor-on when the port
is released, so the motor can only be held off while a process owns the port.

Lifecycle:
  Boot → this service starts → motor off
  systemctl start mybot-lidar.service → Conflicts= stops this service →
      port released → ROS driver takes port → motor runs during scan
  systemctl stop mybot-lidar.service → ROS driver exits →
      ExecStopPost restarts this service → port reclaimed → motor off

Must remain running. Stopped automatically by mybot-lidar.service via
Conflicts= before the ROS LiDAR driver takes the port.
"""
import signal
import sys
import time
import serial

PORT = "/dev/rplidar"
BAUD = 115200


def main():
    try:
        s = serial.Serial(PORT, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"[lidar-motor-off] cannot open {PORT}: {e}", file=sys.stderr)
        sys.exit(1)

    s.dtr = False
    s.rts = False

    def shutdown(sig, frame):
        print("[lidar-motor-off] releasing port", flush=True)
        s.close()
        sys.exit(0)

    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)

    while True:
        s.dtr = False
        s.rts = False
        print(
            f"[lidar-motor-off] DTR={s.dtr} RTS={s.rts} — verify motor state physically",
            flush=True,
        )
        time.sleep(1)


if __name__ == "__main__":
    main()
