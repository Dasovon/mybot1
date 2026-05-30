#!/usr/bin/env python3
"""
OLED display daemon for mybot1 — systemd service on Pi.
Reads Pi system stats via psutil and battery data directly from the
INA219 on the Pi I2C-1 bus (0x40) via pi-ina219 / smbus2.

Display: Waveshare 2.42" SSD1309, 128x64, SPI0
GPIO: DC=GPIO25, RST=GPIO27 (BCM, gpiochip4 on Pi 5)
Battery: INA219 at 0x40 on Pi I2C-1 (GPIO 2/3), polled at 1 Hz
"""

import signal
import socket
import sys
import threading
import time

import lgpio
import psutil
from ina219 import INA219
from luma.core.interface.serial import spi
from luma.oled.device import ssd1309
from PIL import Image, ImageDraw, ImageFont

GPIO_DC   = 25
GPIO_RST  = 27
SPI_PORT  = 0
SPI_DEV   = 0
GPIOCHIP  = 4
UPDATE_HZ = 2

BAT_FULL_V   = 12.6
BAT_CUTOFF_V = 9.9


class LGPIOAdapter:
    """Minimal RPi.GPIO-compatible adapter using lgpio for Pi 5 (gpiochip4)."""
    BCM = 11
    OUT = 0
    HIGH = 1
    LOW = 0

    def __init__(self):
        self._handle = lgpio.gpiochip_open(GPIOCHIP)

    def setmode(self, mode):
        pass

    def setup(self, pin, mode):
        lgpio.gpio_claim_output(self._handle, pin)

    def output(self, pin, value):
        lgpio.gpio_write(self._handle, pin, value)

    def cleanup(self):
        lgpio.gpiochip_close(self._handle)


class BatteryReader:
    """Reads voltage and current directly from INA219 on Pi I2C-1 (0x40).
    Polls at 1 Hz in a background thread. Available from first boot,
    independent of ROS2 and micro-ROS session state.
    """

    def __init__(self):
        self.voltage = 0.0
        self.current = 0.0  # Amps
        self._last_read = 0.0
        self._ina = None
        threading.Thread(target=self._run, daemon=True).start()

    def _run(self):
        while True:
            try:
                if self._ina is None:
                    self._ina = INA219(0.1, busnum=1)
                    self._ina.configure()
                self.voltage = self._ina.voltage()
                self.current = self._ina.current() / 1000.0  # mA → A
                self._last_read = time.monotonic()
            except Exception:
                self._ina = None  # force re-init on next iteration
            time.sleep(1.0)

    @property
    def fresh(self):
        return self._last_read > 0 and (time.monotonic() - self._last_read < 3.0)

    @property
    def pct(self):
        p = (self.voltage - BAT_CUTOFF_V) / (BAT_FULL_V - BAT_CUTOFF_V) * 100.0
        return max(0, min(100, int(p)))


def get_ip() -> str:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return '?.?.?.?'


def _draw_battery_bar(draw, x, y, w, h, pct):
    cap_w, cap_h = 3, h // 2
    cap_x = x + w
    cap_y = y + (h - cap_h) // 2
    draw.rectangle([x, y, x + w - 1, y + h - 1], outline=1, fill=0)
    draw.rectangle([cap_x, cap_y, cap_x + cap_w - 1, cap_y + cap_h - 1], fill=1)
    filled = max(0, int((w - 2) * pct / 100))
    if filled > 0:
        draw.rectangle([x + 1, y + 1, x + 1 + filled - 1, y + h - 2], fill=1)


def render(device, bat: BatteryReader):
    try:
        font = ImageFont.truetype(
            '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 9)
    except IOError:
        font = ImageFont.load_default()

    hostname = socket.gethostname()

    while True:
        img  = Image.new('1', (device.width, device.height), 0)
        draw = ImageDraw.Draw(img)

        cpu    = psutil.cpu_percent(interval=None)
        ram    = psutil.virtual_memory().percent
        temps  = psutil.sensors_temperatures().get('cpu_thermal', [])
        temp_c = temps[0].current if temps else 0.0
        ip     = get_ip()

        draw.text((0, 0), f'{hostname}  {ip}', font=font, fill=1)

        bar_w, bar_h = 96, 10
        pct = bat.pct if bat.fresh else 0
        _draw_battery_bar(draw, 0, 12, bar_w, bar_h, pct)
        pct_str = f'{pct:3d}%' if bat.fresh else ' -- '
        draw.text((bar_w + 5, 13), pct_str, font=font, fill=1)

        if bat.fresh:
            row3 = f'{bat.voltage:.1f}V {bat.current:.2f}A T:{temp_c:.0f}C'
        else:
            row3 = f'no battery  T:{temp_c:.0f}C'
        draw.text((0, 24), row3, font=font, fill=1)

        draw.text((0, 36), f'CPU:{cpu:3.0f}%   RAM:{ram:3.0f}%', font=font, fill=1)

        uptime_s = int(time.monotonic())
        h, rem = divmod(uptime_s, 3600)
        m = rem // 60
        bat_str = 'BAT:OK' if bat.fresh else 'BAT:--'
        draw.text((0, 50), f'{bat_str}   up {h}h{m:02d}m', font=font, fill=1)

        device.display(img)
        time.sleep(1.0 / UPDATE_HZ)


def main():
    signal.signal(signal.SIGTERM, lambda *_: sys.exit(0))

    serial_obj = spi(device=SPI_DEV, port=SPI_PORT,
                     gpio_DC=GPIO_DC, gpio_RST=GPIO_RST,
                     gpio=LGPIOAdapter())
    device = ssd1309(serial_obj, width=128, height=64)
    device.contrast(128)

    bat = BatteryReader()
    render(device, bat)


if __name__ == '__main__':
    main()
