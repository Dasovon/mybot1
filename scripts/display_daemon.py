#!/usr/bin/env python3
"""
OLED display daemon for mybot1.
Runs as a ROS2-independent systemd service on the Pi.
Shows system stats (always) + ESP32 battery telemetry (when available).

Display: Waveshare 2.42" SSD1309, 128x64, SPI0
ESP32 JSON source: /dev/ttyUSB0 (CH340 UART0, 9600 baud), format:
    {"v":12.34,"i":1.23,"p":15.16,"ok":1,"ts":12345}
"""

import json
import socket
import threading
import time

import psutil
from luma.core.interface.serial import spi
from luma.oled.device import ssd1309
from PIL import Image, ImageDraw, ImageFont

# SPI pins (BCM numbering)
GPIO_DC  = 25
GPIO_RST = 27
SPI_PORT = 0
SPI_DEV  = 0

SERIAL_PORT = '/dev/ttyUSB0'
SERIAL_BAUD = 9600
UPDATE_HZ   = 2


def get_ip() -> str:
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return '?.?.?.?'


class SerialReader:
    """Reads ESP32 JSON telemetry from CH340 UART in a background thread."""

    def __init__(self):
        self.data = {}
        self.connected = False
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def _run(self):
        while True:
            try:
                import serial
                with serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=1) as ser:
                    self.connected = True
                    for line in ser:
                        try:
                            self.data = json.loads(line.decode().strip())
                        except (json.JSONDecodeError, UnicodeDecodeError):
                            pass
            except Exception:
                self.connected = False
                self.data = {}
                time.sleep(5)


def render(device, esp: SerialReader):
    try:
        font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 11)
        font_sm = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 10)
    except IOError:
        font = ImageFont.load_default()
        font_sm = font

    while True:
        img = Image.new('1', (device.width, device.height), 0)
        draw = ImageDraw.Draw(img)

        cpu   = psutil.cpu_percent(interval=None)
        ram   = psutil.virtual_memory().percent
        temp  = psutil.sensors_temperatures().get('cpu_thermal', [{}])[0]
        temp_c = temp.current if hasattr(temp, 'current') else 0.0
        ip    = get_ip()

        # Line 1: IP address
        draw.text((0, 0),  f'{ip}', font=font, fill=1)

        # Line 2: CPU + temp
        draw.text((0, 14), f'CPU:{cpu:4.0f}%  {temp_c:.0f}C', font=font, fill=1)

        # Line 3: RAM
        draw.text((0, 27), f'RAM:{ram:4.0f}%', font=font, fill=1)

        # Line 4: battery (ESP32 JSON) or placeholder
        if esp.connected and esp.data:
            v = esp.data.get('v', 0.0)
            i = esp.data.get('i', 0.0)
            p = esp.data.get('p', 0.0)
            draw.text((0, 40), f'{v:.2f}V {i:.2f}A {p:.1f}W', font=font_sm, fill=1)
        else:
            draw.text((0, 40), f'ESP32: no data', font=font_sm, fill=1)

        # Line 5: status bar
        status = 'ROS:?'
        draw.text((0, 53), status, font=font_sm, fill=1)

        device.display(img)
        time.sleep(1.0 / UPDATE_HZ)


def main():
    serial_obj = spi(device=SPI_DEV, port=SPI_PORT, gpio_DC=GPIO_DC, gpio_RST=GPIO_RST)
    device = ssd1309(serial_obj, width=128, height=64)
    device.contrast(128)

    esp = SerialReader()
    render(device, esp)


if __name__ == '__main__':
    main()
