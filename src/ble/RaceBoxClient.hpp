// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

// Forward declarations to avoid heavy includes in header
#include <functional>
class NimBLEClient;
class NimBLERemoteCharacteristic;

namespace ktsu { namespace racebox { namespace ble {

struct RaceboxData {
  // Minimal sample fields; extend per protocol rev 8
  float speedKmh = 0.0f;
  float altitudeM = 0.0f;
  uint32_t sats = 0;
};

class RaceBoxClient {
 public:
  using TelemetryListener = std::function<void(const RaceboxData&)>;

  void begin();
  void loop();

  void setTelemetryListener(TelemetryListener listener) { telemetryListener_ = listener; }
  void handleNotify(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool isNotify);

 private:
  void connectIfNeeded();

  NimBLEClient* client_ = nullptr;
  NimBLERemoteCharacteristic* txChr_ = nullptr; // notify
  NimBLERemoteCharacteristic* rxChr_ = nullptr; // write
  TelemetryListener telemetryListener_;
  uint32_t lastReconnectAttemptMs_ = 0;
};

} } } // namespaces


