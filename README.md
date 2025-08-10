# RaceBox Mini Interface (ESP-IDF)

Firmware to connect to a RaceBox Mini via BLE and present telemetry on a display with a rotary encoder.

This project now uses ESP-IDF (no Arduino). Display and BLE client are currently stubs; flesh out as needed.

Quick start:
- Install ESP-IDF v5.x tools.
- Update `main/config/Pins.hpp` to match wiring.
- Replace UUID placeholders in `main/config/BleUuids.hpp` per the PDF.
- Build, flash, and monitor:
  - `idf.py set-target esp32s3`
  - `idf.py build`
  - `idf.py -p <COMx> flash monitor`

Or use the PowerShell helper (auto-installs ESP-IDF if needed):

```
./build.ps1 -Target esp32s3 -Flash -Monitor -Port COM7
```

Notes:
- BLE client is currently a stub; integrate NimBLE central (IDF) to scan and subscribe to UART service.
- Display is a stub; choose an IDF-friendly driver (e.g., LVGL + ILI9488 panel driver) and wire SPI + backlight.


