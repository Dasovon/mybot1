#!/usr/bin/env python3
"""Diagnostic motor-stop holder for the RPLidar A1.

Measured hypothesis under test:
    The motor may respond to a DTR=True -> DTR=False transition while the
    serial port remains open, rather than to the steady DTR=False level alone.
"""

import signal
import sys
import time

import serial

PORT = "/dev/rplidar"
BAUD = 115200


def main() -> int:
    try:
        s = serial.Serial(PORT, BAUD, timeout=1)
    except serial.SerialException as exc:
        print(f"[lidar-motor-off] cannot open {PORT}: {exc}", file=sys.stderr, flush=True)
        return 1

    def shutdown(sig, frame):
        del sig, frame
        print("[lidar-motor-off] releasing port", flush=True)
        s.close()
        raise SystemExit(0)

    signal.signal(signal.SIGTERM, shutdown)
    signal.signal(signal.SIGINT, shutdown)

    s.rts = False

    s.dtr = True
    print("[lidar-motor-off] DTR=True precondition applied", flush=True)
    time.sleep(0.5)

    s.dtr = False
    print(
        "[lidar-motor-off] DTR=True -> False transition applied; "
        "verify whether motor stops and remains stopped",
        flush=True,
    )

    while True:
        time.sleep(1)


if __name__ == "__main__":
    raise SystemExit(main())
