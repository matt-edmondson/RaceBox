// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

namespace ktsu {
namespace Pins {
// SPI TFT (adjust for your wiring)
static const int tftMosi = 11;
static const int tftMiso = 13;
static const int tftSclk = 12;
static const int tftCs   = 10;
static const int tftDc   = 9;
static const int tftRst  = 8;
static const int tftBl   = 7;

// Rotary Encoder
static const int encoderPinA = 2;
static const int encoderPinB = 1;
static const int encoderButtonPin = 3;
}
} // namespace ktsu


