#!/usr/bin/env python3
"""
Open-loop motor validation — requires firmware/esp32_test firmware on ESP32.

Protocol:
  TX  D,<left>,<right>\\n          duty integers -255..255
  RX  E,<vel_l>,<vel_r>,<pos_l>,<pos_r>\\n   at 50 Hz  (rad/s, rad)

Usage (on Pi):
  python3 motor_test.py [/dev/ttyACM0]
"""

import os
import sys
import time
import signal
import statistics
import subprocess
import serial

# Usage:  motor_test.py [port] [p]
#   port defaults to /dev/ttyACM0
#   add "p" as any argument to run the P-controller test instead of open-loop
_args = sys.argv[1:]
MODE  = "p" if "p" in _args else "open"
PORT  = next((a for a in _args if a != "p"), "/dev/ttyACM0")
BAUD      = 115200
SETTLE_S  = 1.0   # time at duty before collecting — lets motor ramp up
COLLECT_S = 3.0   # data window per step


# ── Cleanup ────────────────────────────────────────────────────────────────

def cleanup_port(port: str) -> None:
    """Kill any process holding the serial port before opening it."""
    print(f"Checking for processes using {port} ...")

    # Stop systemd services that commonly hold ttyACM0
    for svc in ["microros-agent.service", "robot-launch.service"]:
        r = subprocess.run(
            ["sudo", "systemctl", "stop", svc],
            capture_output=True, timeout=5
        )
        if r.returncode == 0:
            print(f"  Stopped {svc}")

    # Kill known ROS zombie processes by name.
    # Each pkill call is narrow so it does not match itself or this script.
    zombies = [
        "micro_ros_agent",
        "battery_publisher",
        "ros2_bag_record",   # intentional underscore — avoids self-match
    ]
    # Use pgrep to find PIDs, then kill individually (avoids pkill self-match issue)
    our_pid = os.getpid()
    for pattern in ["micro_ros_agent", "battery_publisher"]:
        r = subprocess.run(["pgrep", "-f", pattern], capture_output=True)
        if r.returncode == 0:
            for pid_str in r.stdout.decode().split():
                pid = int(pid_str)
                if pid != our_pid:
                    try:
                        os.kill(pid, signal.SIGKILL)
                        print(f"  Killed {pattern} (pid {pid})")
                    except (ProcessLookupError, PermissionError):
                        pass

    # Kill "ros2 bag" separately using pgrep to avoid self-match
    r = subprocess.run(["pgrep", "-f", "ros2.*bag"], capture_output=True)
    if r.returncode == 0:
        for pid_str in r.stdout.decode().split():
            pid = int(pid_str)
            if pid != our_pid:
                try:
                    os.kill(pid, signal.SIGKILL)
                    print(f"  Killed ros2 bag record (pid {pid})")
                except (ProcessLookupError, PermissionError):
                    pass

    # Hard fallback: fuser kills whatever still holds the port
    r = subprocess.run(["sudo", "fuser", "-k", port], capture_output=True, timeout=5)
    if r.returncode == 0:
        print(f"  fuser released {port}")

    time.sleep(1)   # let the port fully release before opening
    print("Port clear.\n")


# ── Serial helpers ─────────────────────────────────────────────────────────

def open_port(port: str) -> serial.Serial:
    s = serial.Serial(port, BAUD, timeout=0.5)
    time.sleep(2)               # wait for ESP32 boot + READY line
    s.reset_input_buffer()
    return s


def send(s: serial.Serial, left: int, right: int) -> None:
    s.write(f"D,{left},{right}\n".encode())


def drain(s: serial.Serial, seconds: float,
          left_duty: int = 0, right_duty: int = 0) -> list:
    """Read E lines for `seconds`. Returns list of (vel_l, vel_r, pos_l, pos_r).
    If left_duty/right_duty are nonzero, refreshes the motor command every 200 ms
    so the firmware watchdog (500 ms) never fires during the window."""
    samples = []
    deadline   = time.time() + seconds
    last_send  = 0.0
    REFRESH_S  = 0.2   # must be < WATCHDOG_MS (500 ms) on the firmware side
    while time.time() < deadline:
        now = time.time()
        if (left_duty or right_duty) and now - last_send >= REFRESH_S:
            send(s, left_duty, right_duty)
            last_send = now
        line = s.readline().decode(errors="ignore").strip()
        if line.startswith("E,"):
            parts = line.split(",")
            if len(parts) == 5:
                try:
                    samples.append(tuple(float(x) for x in parts[1:]))
                except ValueError:
                    pass
    return samples


def explicit_stop(s: serial.Serial) -> None:
    for _ in range(5):
        send(s, 0, 0)
        time.sleep(0.05)


# ── Test ───────────────────────────────────────────────────────────────────

def run_test() -> None:
    cleanup_port(PORT)

    print(f"Opening {PORT} at {BAUD} baud ...")
    s = open_port(PORT)
    drain(s, 0.5)   # discard READY + startup noise
    print("Connected.\n")

    results = {}

    # Duty steps: 25 % (64), 35 % (90), 50 % (128)
    for duty in [64, 90, 128]:
        pct = round(duty / 255 * 100)
        print(f"{'─'*56}")
        print(f"  Duty {duty}/255  (~{pct}%)  —  settle {SETTLE_S}s, collect {COLLECT_S}s")

        send(s, duty, duty)
        drain(s, SETTLE_S, duty, duty)          # keep motor alive during settle

        samples = drain(s, COLLECT_S, duty, duty)

        explicit_stop(s)
        time.sleep(0.4)

        if not samples:
            print("  ERROR: no encoder data received")
            print("  Check: firmware flashed? ESP32 reset after flash?\n")
            continue

        l_vels    = [x[0] for x in samples]
        r_vels    = [x[1] for x in samples]
        l_pos_end = samples[-1][2]
        r_pos_end = samples[-1][3]

        l_mean = statistics.mean(l_vels)
        r_mean = statistics.mean(r_vels)
        l_std  = statistics.stdev(l_vels) if len(l_vels) > 1 else 0.0
        r_std  = statistics.stdev(r_vels) if len(r_vels) > 1 else 0.0
        asym   = l_mean - r_mean

        results[duty] = dict(l_mean=l_mean, r_mean=r_mean,
                             l_std=l_std,   r_std=r_std,
                             n=len(samples))

        print(f"\n  n={len(samples)} samples")
        print(f"  L  mean={l_mean:+6.2f} rad/s  std={l_std:.2f}  pos_end={l_pos_end:.2f} rad")
        print(f"  R  mean={r_mean:+6.2f} rad/s  std={r_std:.2f}  pos_end={r_pos_end:.2f} rad")
        print(f"  Asymmetry  L−R = {asym:+.2f} rad/s")

        checks = [
            ("L moves  (|mean| > 0.5 rad/s)",  abs(l_mean) > 0.5),
            ("R moves  (|mean| > 0.5 rad/s)",  abs(r_mean) > 0.5),
            ("L sign positive (forward)",       l_mean > 0),
            ("R sign positive (forward)",       r_mean > 0),
            ("Symmetry  |L−R| < 1.5 rad/s",    abs(asym) < 1.5),
        ]
        for label, passed in checks:
            print(f"  {'PASS' if passed else 'FAIL'}  {label}")
        print()

    # ── Summary table ───────────────────────────────────────────────────────
    if results:
        print("=" * 56)
        print("  SUMMARY")
        print(f"  {'Duty':>6}  {'L mean':>8}  {'R mean':>8}  {'L−R':>6}  {'L std':>6}  {'R std':>6}")
        for duty, r in results.items():
            pct = round(duty / 255 * 100)
            print(f"  {duty:>3}/255  {r['l_mean']:>8.2f}  {r['r_mean']:>8.2f}  "
                  f"{r['l_mean']-r['r_mean']:>+6.2f}  {r['l_std']:>6.2f}  {r['r_std']:>6.2f}")
        print("=" * 56)

    explicit_stop(s)
    s.close()
    print("\nMotors stopped. Done.")


def run_p_test() -> None:
    """
    Python-side P-only velocity controller.
    Reads vel from firmware at 50 Hz, computes duty, sends D command each cycle.

    Goal: confirm a simple feedback loop is stable on this hardware.
    Not expected to achieve zero steady-state error (no integrator).
    """
    TARGET    = 3.0    # rad/s — within measured range (2.6 @ 25%, 5.9 @ 50%)
    KP        = 20.0   # duty units per rad/s error
    FF        = 64     # feedforward: minimum duty to start motion (from open-loop test)
    DURATION  = 8.0    # seconds total
    SS_WINDOW = 5.0    # last N seconds used for steady-state stats

    cleanup_port(PORT)
    print(f"Opening {PORT} at {BAUD} baud ...")
    s = open_port(PORT)
    drain(s, 0.5)
    print("Connected.\n")

    print(f"P-controller: target={TARGET} rad/s  Kp={KP}  FF={FF}  duration={DURATION}s")
    print(f"{'─'*56}")
    print(f"  {'t':>5}  {'vel_L':>7}  {'vel_R':>7}  {'duty_L':>7}  {'duty_R':>7}")

    samples = []   # (t, vel_l, vel_r, duty_l, duty_r)
    t_start = time.time()

    while True:
        t = time.time() - t_start
        if t >= DURATION:
            break

        line = s.readline().decode(errors="ignore").strip()
        if not line.startswith("E,"):
            continue
        parts = line.split(",")
        if len(parts) != 5:
            continue
        try:
            vel_l, vel_r = float(parts[1]), float(parts[2])
        except ValueError:
            continue

        # Independent P + feedforward per wheel
        def p_duty(vel: float) -> int:
            raw = FF + int(KP * (TARGET - vel))
            return max(0, min(255, raw))

        dl = p_duty(vel_l)
        dr = p_duty(vel_r)
        send(s, dl, dr)
        samples.append((t, vel_l, vel_r, dl, dr))

        if len(samples) % 25 == 0:   # print every ~0.5 s
            print(f"  {t:5.1f}s  {vel_l:+7.2f}  {vel_r:+7.2f}  {dl:7d}  {dr:7d}")

    explicit_stop(s)
    s.close()

    # ── Steady-state analysis ────────────────────────────────────────────────
    ss = [(vl, vr, dl, dr) for t, vl, vr, dl, dr in samples if t > DURATION - SS_WINDOW]
    if not ss:
        print("No steady-state samples.")
        return

    l_vels  = [x[0] for x in ss]
    r_vels  = [x[1] for x in ss]
    l_dutys = [x[2] for x in ss]
    r_dutys = [x[3] for x in ss]

    l_mean = statistics.mean(l_vels)
    r_mean = statistics.mean(r_vels)
    l_std  = statistics.stdev(l_vels)
    r_std  = statistics.stdev(r_vels)

    print(f"\n{'='*56}")
    print(f"  Steady-state (last {SS_WINDOW:.0f}s, n={len(ss)} samples)")
    print(f"  Target: {TARGET} rad/s")
    print(f"{'─'*56}")
    print(f"  L vel:  mean={l_mean:+.2f}  std={l_std:.2f}  err={l_mean-TARGET:+.2f} ({(l_mean-TARGET)/TARGET*100:+.0f}%)")
    print(f"  R vel:  mean={r_mean:+.2f}  std={r_std:.2f}  err={r_mean-TARGET:+.2f} ({(r_mean-TARGET)/TARGET*100:+.0f}%)")
    print(f"  L duty: mean={statistics.mean(l_dutys):.0f}")
    print(f"  R duty: mean={statistics.mean(r_dutys):.0f}")
    print(f"  L-R vel asym: {l_mean-r_mean:+.2f} rad/s")
    print(f"{'='*56}")
    print("\nMotors stopped. Done.")


if __name__ == "__main__":
    if MODE == "p":
        run_p_test()
    else:
        run_test()
