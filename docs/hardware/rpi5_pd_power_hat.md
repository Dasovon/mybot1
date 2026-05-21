# RPI5 PD Power Hat — Power Distribution Board

## Role in This Project

The RPI5 PD Power Hat is the power distribution board for this build. It accepts a battery input (9–24V DC) and delivers regulated 5V/8A to the Raspberry Pi 5 via USB PD 3.0, plus a raw VIN passthrough for motor power.

---

## Key Specs (from board silkscreen)

| Property | Value |
|---|---|
| Model | P01 |
| Input | USB PD (default 15V) or DC barrel (9–24V) |
| Output 1 | USB PD 3.0, 5.15V / 5A (to Raspberry Pi 5 USB-C) |
| Output 2 | 5V VBUS (screw terminal / header) |
| Max output power | 40W (5V × 8A) |
| VIN passthrough | Raw input voltage on screw terminal (for motor power) |
| AlwaysON jumper | Enable/Disable — controls power-on behavior |
| PowerON connector | Soft power control |

---

## Board Connectors

### INPUT (right side)
- USB-C input port — accepts USB PD source (charger, battery bank) at negotiated voltage
- DC barrel jack — accepts 9–24V DC from LiPo/Li-ion battery pack

### OUTPUT (bottom left)
- USB-C output port — delivers 5.15V / 5A (USB PD 3.0) to **Raspberry Pi 5 USB-C power port**

### Screw Terminal / Header (top right — labeled OUTPUT)
| Terminal | Signal |
|---|---|
| +5V | Regulated 5V output |
| GND | Ground |
| GND | Ground |
| VIN | Raw input voltage passthrough (= battery voltage) |

### VIN Header (bottom center)
Duplicate VIN and GND pads — additional solder points for motor power wiring.

### AlwaysON Jumper (top left)
| Position | Behavior |
|---|---|
| Enable | Board always on when input power is present |
| Disable | Power controlled by PowerON connector |

### PD Voltage Selection (resistor pads, right side)
| Resistor | Negotiated PD Voltage |
|---|---|
| 6.8 kΩ | 9V |
| 24 kΩ | 12V |
| 56 kΩ | 15V (default) |
| NC | 20V |

---

## Power Architecture (Pi 5 Build)

```
LiPo Battery (3S = ~12V, or compatible 9–24V source)
    │
[XT60 anti-spark connector]  ← prevents arcing on connect; required with caps on TB6612 VM
    │
[Main switch]
    │
[INA219 VIN+ → shunt → INA219 VIN−]  ← measures total current before rail split
    │
    ├── RPI5 PD Power Hat INPUT (DC barrel)
            │
            ├── OUTPUT USB-C (5.15V / 5A, USB PD 3.0)
            │       └── Raspberry Pi 5 USB-C power port
            │               ├── Pi USB-A → ESP32-S3    (power + micro-ROS serial)
            │               ├── Pi USB-A → RPLidar A1  (power + data, USB 2.0)
            │               └── Pi USB-A → RealSense D435 (power + data, USB 3.0)
            │
            └── VIN screw terminal (raw battery voltage)
                    └── TB6612FNG VM pin (motor power)

TB6612 logic VCC → ESP32 3V3 pin (not from hat directly)
```

---

## Notes

- The Pi 5 **requires USB PD negotiation** — the hat's USB-C output handles it automatically. Do not use a standard USB-C cable.
- The VIN passthrough is unregulated motor power — connects to TB6612 VM, not logic components.
- Add an inline fuse on the battery positive wire before the DC barrel input. No onboard fuse on this board.

---

## Notes

- **Anti-spark connector:** Use an XT60 anti-spark connector on the battery. The 1000µF cap on TB6612 VM will arc and degrade connectors without it on every power-up.
- **Battery sag:** A 3S LiPo at low charge hits ~9.6V at rest; under motor load it can sag below 9V and brown out the Pi. Set a conservative low-voltage alarm (10V under load) or add an MT3608 boost converter between the battery and the DC barrel input (set to 15V) to eliminate sag entirely.
- **Battery chemistry:** Confirm voltage stays within 9–24V at both full charge and cutoff. A 3S LiPo (12.6V full, 9.9V cutoff) fits well. A 4S LiPo (16.8V full) also fits.
- **PD voltage selection:** When using the DC barrel (not USB PD input), PD negotiation is not involved — the converter regulates directly from barrel voltage to 5V.
- **5V rail budget:** Pi 5 (~25W) + RealSense (~4.5W) + RPLidar (~2W) + ESP32 (~0.5W) = ~32W peak. The hat's 40W cap gives ~8W of headroom — adequate for this build.
- **Fuse:** Add an inline fuse on the battery positive wire before the hat's DC barrel input. No onboard fuse is visible on the board.
