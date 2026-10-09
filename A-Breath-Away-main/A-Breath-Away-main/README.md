

An Arduino library for controlling pilot wire electric heaters via Zigbee with ESP32-H2 or ESP32-C6.

[![GitHub release (latest by date including pre-releases)](https://img.shields.io/github/v/release/epsilonrt/ZigbeePilotWireControl?include_prereleases)](https://github.com/epsilonrt/ZigbeePilotWireControl/releases)
[![Framework](https://img.shields.io/badge/Framework-Arduino-blue)](https://www.arduino.cc/)
[![Platform ESP32](https://img.shields.io/badge/Platform-Espressif32-orange)](https://www.espressif.com/en/products/socs/esp32)  
[![Build](https://github.com/epsilonrt/ZigbeePilotWireControl/actions/workflows/build.yml/badge.svg)](https://github.com/epsilonrt/ZigbeePilotWireControl/actions/workflows/build.yml)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/epsilonrt/library/ZigbeePilotWireControl.svg)](https://registry.platformio.org/libraries/epsilonrt/ZigbeePilotWireControl)
[![Arduino Registry](https://www.ardu-badge.com/badge/ZigbeePilotWireControl.svg)](https://www.arduinolibraries.info/libraries/ZigbeePilotWireControl)  

-------

## Presence Monitor Wiring

The default root PlatformIO environment is `presence_xiao_esp32c6_hlk_ld2410b`, which targets a Seeed Studio XIAO ESP32-C6 with an HLK-LD2410-series radar attached.

### HLK-LD2410C Signal Behavior

- `VCC`: connect to a stable `5V` supply with at least `200 mA` available.
- `GND`: connect to the system ground shared with the XIAO ESP32-C6.
- `OUT`: digital presence output. The module drives this line `HIGH` at about `3.3V` when presence is detected and `LOW` (`0V`) when the area is clear.
- `TX` / `RX`: optional UART pins for configuration and detailed radar data.

### Seeed XIAO ESP32-C6 Breakout Wiring

- `LD2410C VCC` -> `5V`
- `LD2410C GND` -> `GND`
- `LD2410C OUT` -> any XIAO `3.3V`-safe digital input if you want direct hardware presence sensing
- `LD2410C TX` -> XIAO `GPIO17` (`RADAR_UART_RX_PIN` in `platformio.ini`)
- `LD2410C RX` -> XIAO `GPIO16` (`RADAR_UART_TX_PIN` in `platformio.ini`)

This matches the current root configuration in `platformio.ini`. If you wire the `OUT` pin, treat it as a logic output only: `LOW` means no presence, `HIGH` means moving or stationary human presence detected.

### RYLR Note

If you are also using an `RYLR` module on the same Seeed breakout, document it as a separate UART/device connection in your hardware notes and avoid reusing the radar UART pins above unless you intentionally remap them in `platformio.ini`.
