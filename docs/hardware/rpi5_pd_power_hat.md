# RPI5 PD Power Hat — Power Distribution Board

## Role in This Project

The RPI5 PD Power Hat is the primary power distribution board for the new Pi 5 build. It replaces the DFR0205 buck converter used on the previous Pi 4 robot. It accepts a battery input (9–24V DC) and delivers regulated 5V/8A to the Raspberry Pi 5 via USB PD 3.0, plus a raw VIN passthrough for motor power.

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
    └── RPI5 PD Power Hat INPUT (DC barrel)
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

## Wiring Notes

- The Pi 5 **requires USB PD negotiation** to receive full power — do not use a standard USB-C cable without PD. The hat's USB-C output handles the PD handshake automatically.
- The VIN passthrough provides raw battery voltage to the TB6612 VM pin — this is unregulated motor power.
- All grounds (battery −, ESP32 GND, TB6612 GND, Pi GND via hat) must share a common ground rail.

---

## Suggestions / Audit Notes

- **Battery chemistry:** Confirm your battery voltage stays within the 9–24V DC input range at both full charge and discharge cutoff. A 3S LiPo (12.6V full, 9.9V cutoff) fits well. A 4S LiPo (16.8V full) also fits.
- **PD voltage selection:** Default 15V input from USB PD. If using the DC barrel from a 12V LiPo, PD negotiation is not involved — the barrel voltage goes straight to the converter.
- **5V rail budget:** 5V × 8A = 40W total. Pi 5 can draw up to 25W under heavy load. RealSense adds ~4.5W. RPLIDAR adds ~2W. ESP32 adds ~0.5W. Budget is tight under full AI load — monitor the hat's thermal performance.
- **No onboard fuse visible:** Add an inline fuse on the battery positive wire before the hat's DC barrel input.
