// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <cstdint>

namespace ktsu { namespace racebox { namespace config {

enum class SpeedUnits : uint8_t { Kmh = 0, Mph = 1 };

// User-adjustable settings, persisted to NVS so they survive a power cycle.
//
// The value semantics and conversions here are pure and unit tested; only
// load()/save() touch ESP-IDF.
class Settings {
 public:
  static constexpr uint8_t kMinBrightness = 10;   // never let the user blank the screen
  static constexpr uint8_t kMaxBrightness = 100;
  static constexpr uint8_t kBrightnessStep = 10;

  SpeedUnits speedUnits() const { return speedUnits_; }
  void setSpeedUnits(SpeedUnits units) { speedUnits_ = units; }
  void toggleSpeedUnits() {
    speedUnits_ = (speedUnits_ == SpeedUnits::Kmh) ? SpeedUnits::Mph : SpeedUnits::Kmh;
  }

  uint8_t brightness() const { return brightness_; }
  void setBrightness(uint8_t percent);
  // Nudge brightness by +/- one step, clamped to the legal range.
  void adjustBrightness(int steps);

  // Convert a km/h speed (the wire unit) into the user's chosen display unit.
  float displaySpeed(float speedKmh) const;
  const char* speedUnitLabel() const;

  // Persistence. Both are no-ops when built without ESP-IDF's NVS.
  void load();
  void save() const;

 private:
  SpeedUnits speedUnits_ = SpeedUnits::Kmh;
  uint8_t brightness_ = 100;
};

} } } // namespaces
