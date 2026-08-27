// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <cstdint>

namespace ktsu { namespace racebox { namespace ble {

// Fix quality reported in the RaceBox data message (UBX "fix status" byte).
enum class FixType : uint8_t {
  NoFix = 0,
  DeadReckoning = 1,
  Fix2D = 2,
  Fix3D = 3,
  GnssPlusDeadReckoning = 4,
  TimeOnly = 5,
};

// Full decode of the RaceBox data message (class 0xFF, id 0x01, 80-byte payload).
//
// Field offsets follow the published RaceBox BLE protocol. Values are converted
// out of the wire's fixed-point integers into natural units at parse time so
// consumers never deal with scaling factors.
struct RaceboxData {
  // --- Timing ---
  uint32_t iTowMs = 0;        // GPS time of week, milliseconds
  uint16_t year = 0;
  uint8_t month = 0;
  uint8_t day = 0;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  int32_t nanoseconds = 0;    // fractional part of `second`, may be negative
  uint32_t timeAccuracyNs = 0;
  uint8_t validityFlags = 0;
  uint8_t dateTimeFlags = 0;

  // --- Fix ---
  FixType fixType = FixType::NoFix;
  uint8_t fixStatusFlags = 0;
  uint8_t satellites = 0;
  bool fixValid = false;      // derived: fixStatusFlags bit 0 (gnssFixOK)

  // --- Position ---
  double longitudeDeg = 0.0;
  double latitudeDeg = 0.0;
  float wgsAltitudeM = 0.0f;
  float mslAltitudeM = 0.0f;
  float horizontalAccuracyM = 0.0f;
  float verticalAccuracyM = 0.0f;

  // --- Motion ---
  float speedKmh = 0.0f;
  float headingDeg = 0.0f;
  float speedAccuracyKmh = 0.0f;
  float headingAccuracyDeg = 0.0f;
  float pdop = 0.0f;

  // --- Battery (Mini / Mini S: charge level; Micro: input voltage) ---
  uint8_t batteryRaw = 0;
  uint8_t batteryPercent = 0; // batteryRaw & 0x7F
  bool charging = false;      // batteryRaw bit 7
  float inputVoltage = 0.0f;  // batteryRaw / 10.0f, meaningful on RaceBox Micro

  // --- Inertial ---
  float gForceX = 0.0f;       // g
  float gForceY = 0.0f;
  float gForceZ = 0.0f;
  float rotationRateX = 0.0f; // degrees/second
  float rotationRateY = 0.0f;
  float rotationRateZ = 0.0f;
};

} } } // namespaces
