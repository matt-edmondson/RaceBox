// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

namespace ktsu {
namespace Pins {
// SPI TFT (adjust for your wiring)
static const int tftMosi = 11;   // ESP32-S3 default VSPI MOSI
static const int tftMiso = 13;   // optional, not used by some TFTs
static const int tftSclk = 12;   // ESP32-S3 default VSPI SCLK
static const int tftCs   = 10;   // Chip Select
static const int tftDc   = 9;    // Data/Command
static const int tftRst  = 8;    // Reset (or -1 if tied)
static const int tftBl   = 7;    // Backlight (PWM-capable pin recommended)

// Rotary Encoder
static const int encoderPinA = 2;
static const int encoderPinB = 1;
static const int encoderButtonPin = 3;
}
} // namespace ktsu


