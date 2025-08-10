# RaceBox Mini Interface (ESP32-S3)

Firmware to connect to a RaceBox Mini via BLE, render telemetry on a 4.0" 480x320 SPI TFT (ILI9488), and control it using a rotary encoder with button.

Quick start:
- Install PlatformIO.
- Update `src/config/Pins.hpp` to match wiring.
- Replace UUID placeholders in `src/config/BleUuids.hpp` per the PDF.
- Build and upload the `esp32-s3-devkitc-1` environment.


