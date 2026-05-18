# ESP32-S3 Firmware

## Framework

Arduino (via PlatformIO or Arduino IDE)

## Structure

```
firmware/esp32/
├── src/          # Main firmware source (.cpp / .ino)
├── include/      # Header files (.h)
└── lib/          # Local libraries
```

## Key Responsibilities

- PID closed-loop velocity control (100 Hz)
- Encoder interrupt handling
- I2C sensor polling: BNO055, INA219, BME680
- ASCII serial protocol to Raspberry Pi 5
- Safety watchdog (motor stop on timeout)
- Battery cutoff logic

## Serial Protocol

See [docs/hardware/esp32_s3.md](../../docs/hardware/esp32_s3.md) for full protocol reference.
