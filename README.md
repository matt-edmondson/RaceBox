# RaceBox Mini Interface (ESP-IDF)

Firmware for an ESP32-S3 that connects to a [RaceBox Mini](https://www.racebox.pro/)
over BLE and presents its telemetry on an SPI TFT, driven by a rotary encoder.

Built with ESP-IDF v5.x (no Arduino), NimBLE for the BLE central role, and LVGL 9
on an ILI9488 panel.

## Status

| Area | State |
| --- | --- |
| BLE central (scan, connect, discover, subscribe) | Implemented |
| UBX framing + RaceBox data decode | Implemented, unit tested |
| Telemetry / menu / about UI | Implemented |
| Rotary encoder (PCNT quadrature + debounced button) | Implemented |
| Session and lap timing | Implemented, unit tested |
| Settings persistence (units, brightness) | Implemented |

**Not yet validated on hardware.** The logic that can be tested off-target is
covered by the host test suite below, but the firmware has not been run against
a physical RaceBox or panel. Treat the pin defaults, the ILI9488 colour setup,
and the RaceBox payload offsets past speed/altitude as needing a bring-up pass.

## Requirements

- An ESP32-S3 board with **at least 4 MB of flash** (the partition table
  reserves a 3 MB app image).
- An ILI9488 SPI TFT, 480x320 by default.
- A rotary encoder with a push-button.
- ESP-IDF **v5.2.2** (matching CI and `build.ps1`; anything v5.2+ should work).

## Quick start

```sh
idf.py set-target esp32s3
idf.py menuconfig      # optional: RaceBox -> pin assignments
idf.py build
idf.py -p <PORT> flash monitor
```

Or use the PowerShell helper, which installs ESP-IDF if it is missing:

```powershell
./build.ps1 -Flash -Monitor -Port COM7
```

`build.ps1` only runs `idf.py set-target` when the configured target actually
needs to change, so it no longer discards local `menuconfig` edits on every
invocation. Pass `-SetTarget` to force it.

## Configuration

Pin assignments and panel geometry live in Kconfig
(`idf.py menuconfig` -> **RaceBox**), not in source. Defaults:

| Function | GPIO |
| --- | --- |
| Encoder A / B / button | 4 / 5 / 6 |
| TFT MOSI / MISO / SCLK | 11 / 13 / 12 |
| TFT CS / DC / RST / backlight | 10 / 9 / 8 / 7 |

> The encoder defaults moved off GPIO 1/2/3. GPIO 3 is an ESP32-S3 strapping pin
> (JTAG source select) and is a poor choice for a button. When picking your own
> pins, avoid the strapping pins (0, 3, 45, 46), the USB lines (19, 20), and the
> pins reserved for SPI flash and PSRAM (26-37).

`sdkconfig.defaults` and `partitions.csv` are checked in so fresh clones build
identically: they pin the target, a 4 MB flash size, the custom partition table,
NimBLE as the BLE host, and the LVGL colour depth, heap size and fonts the UI
depends on. Re-run `idf.py reconfigure` after pulling changes to either file.

## BLE protocol notes

The device is found by advertised name (`RaceBox ...`) or by the Nordic UART
Service UUID, then telemetry is read from the UART TX characteristic as a
stream of UBX packets.

The UUIDs in `main/config/BleUuids.hpp` are **not placeholders** — they are the
standard Nordic UART Service values that RaceBox devices use, and service
discovery relies on them directly. They should not need editing for a Mini or
Mini S.

RaceBox data messages are UBX class `0xFF`, id `0x01`, with an 80-byte payload.
`UbxParser` reassembles packets across notifications, verifies the Fletcher
checksum, and **discards frames that fail it** rather than decoding them.

## Testing

The parts of the firmware with no ESP-IDF dependency — UBX parsing, lap timing,
menu navigation and settings — are unit tested on the host:

```sh
cmake -S test/host -B build-host
cmake --build build-host
./build-host/racebox_host_tests
```

CI runs these on every push alongside a full ESP-IDF firmware build.

## Layout

```
main/
  ble/     RaceBoxClient (NimBLE central), UbxParser, RaceboxData
  config/  Pins (Kconfig-backed), BleUuids, Settings (NVS-backed)
  io/      EncoderInput (PCNT quadrature + debounced button)
  ui/      Display (LVGL), Menu, LapTimer
  common/  IdfCompat, CriticalSection
test/host/ Host unit tests
```

## Threading

Three tasks matter:

- **NimBLE host task** — runs every BLE callback, decodes UBX, and hands
  telemetry to the display through a spinlock-guarded snapshot.
- **UI task** (`main.cpp`, 8 KB stack) — polls the encoder and owns *every*
  LVGL call.
- **LVGL tick timer** — a 1 ms `esp_timer` calling `lv_tick_inc`.

LVGL is not thread safe, so nothing outside the UI task may call into it.

## License

MIT — see [LICENSE](LICENSE).
