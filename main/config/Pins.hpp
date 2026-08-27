// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

// Pin assignments and panel geometry come from Kconfig (see
// main/Kconfig.projbuild) so wiring can be changed with `idf.py menuconfig`
// rather than by editing source. The fallbacks below keep the headers valid
// when the code is parsed outside an ESP-IDF build.
//
// ESP32-S3 caution: GPIO 0, 3, 45 and 46 are strapping pins, 19/20 are the USB
// D-/D+ lines, and 26-37 are reserved for SPI flash and PSRAM. The defaults
// deliberately avoid all of these.

#ifndef CONFIG_RACEBOX_ENCODER_PIN_A
#define CONFIG_RACEBOX_ENCODER_PIN_A 4
#endif
#ifndef CONFIG_RACEBOX_ENCODER_PIN_B
#define CONFIG_RACEBOX_ENCODER_PIN_B 5
#endif
#ifndef CONFIG_RACEBOX_ENCODER_PIN_BUTTON
#define CONFIG_RACEBOX_ENCODER_PIN_BUTTON 6
#endif
#ifndef CONFIG_RACEBOX_ENCODER_COUNTS_PER_DETENT
#define CONFIG_RACEBOX_ENCODER_COUNTS_PER_DETENT 4
#endif
#ifndef CONFIG_RACEBOX_ENCODER_GLITCH_NS
#define CONFIG_RACEBOX_ENCODER_GLITCH_NS 1000
#endif
#ifndef CONFIG_RACEBOX_BUTTON_DEBOUNCE_MS
#define CONFIG_RACEBOX_BUTTON_DEBOUNCE_MS 25
#endif
#ifndef CONFIG_RACEBOX_BUTTON_LONG_PRESS_MS
#define CONFIG_RACEBOX_BUTTON_LONG_PRESS_MS 600
#endif

#ifndef CONFIG_RACEBOX_TFT_PIN_MOSI
#define CONFIG_RACEBOX_TFT_PIN_MOSI 11
#endif
#ifndef CONFIG_RACEBOX_TFT_PIN_MISO
#define CONFIG_RACEBOX_TFT_PIN_MISO 13
#endif
#ifndef CONFIG_RACEBOX_TFT_PIN_SCLK
#define CONFIG_RACEBOX_TFT_PIN_SCLK 12
#endif
#ifndef CONFIG_RACEBOX_TFT_PIN_CS
#define CONFIG_RACEBOX_TFT_PIN_CS 10
#endif
#ifndef CONFIG_RACEBOX_TFT_PIN_DC
#define CONFIG_RACEBOX_TFT_PIN_DC 9
#endif
#ifndef CONFIG_RACEBOX_TFT_PIN_RST
#define CONFIG_RACEBOX_TFT_PIN_RST 8
#endif
#ifndef CONFIG_RACEBOX_TFT_PIN_BACKLIGHT
#define CONFIG_RACEBOX_TFT_PIN_BACKLIGHT 7
#endif
#ifndef CONFIG_RACEBOX_TFT_WIDTH
#define CONFIG_RACEBOX_TFT_WIDTH 480
#endif
#ifndef CONFIG_RACEBOX_TFT_HEIGHT
#define CONFIG_RACEBOX_TFT_HEIGHT 320
#endif
#ifndef CONFIG_RACEBOX_TFT_PCLK_HZ
#define CONFIG_RACEBOX_TFT_PCLK_HZ 40000000
#endif
#ifndef CONFIG_RACEBOX_TFT_BUFFER_LINES
#define CONFIG_RACEBOX_TFT_BUFFER_LINES 40
#endif

namespace ktsu {
namespace Pins {

// SPI TFT
inline constexpr int tftMosi = CONFIG_RACEBOX_TFT_PIN_MOSI;
inline constexpr int tftMiso = CONFIG_RACEBOX_TFT_PIN_MISO;
inline constexpr int tftSclk = CONFIG_RACEBOX_TFT_PIN_SCLK;
inline constexpr int tftCs   = CONFIG_RACEBOX_TFT_PIN_CS;
inline constexpr int tftDc   = CONFIG_RACEBOX_TFT_PIN_DC;
inline constexpr int tftRst  = CONFIG_RACEBOX_TFT_PIN_RST;
inline constexpr int tftBl   = CONFIG_RACEBOX_TFT_PIN_BACKLIGHT;

// Rotary encoder
inline constexpr int encoderPinA = CONFIG_RACEBOX_ENCODER_PIN_A;
inline constexpr int encoderPinB = CONFIG_RACEBOX_ENCODER_PIN_B;
inline constexpr int encoderButtonPin = CONFIG_RACEBOX_ENCODER_PIN_BUTTON;

} // namespace Pins

namespace Panel {
inline constexpr int width = CONFIG_RACEBOX_TFT_WIDTH;
inline constexpr int height = CONFIG_RACEBOX_TFT_HEIGHT;
inline constexpr int pixelClockHz = CONFIG_RACEBOX_TFT_PCLK_HZ;
inline constexpr int bufferLines = CONFIG_RACEBOX_TFT_BUFFER_LINES;
} // namespace Panel

namespace Input {
inline constexpr int countsPerDetent = CONFIG_RACEBOX_ENCODER_COUNTS_PER_DETENT;
inline constexpr int glitchFilterNs = CONFIG_RACEBOX_ENCODER_GLITCH_NS;
inline constexpr int debounceMs = CONFIG_RACEBOX_BUTTON_DEBOUNCE_MS;
inline constexpr int longPressMs = CONFIG_RACEBOX_BUTTON_LONG_PRESS_MS;
} // namespace Input

} // namespace ktsu
