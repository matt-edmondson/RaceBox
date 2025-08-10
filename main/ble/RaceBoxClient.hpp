// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <functional>
#include <cstdint>

namespace ktsu { namespace racebox { namespace ble {

struct RaceboxData {
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

 private:
  TelemetryListener telemetryListener_;
};

} } } // namespaces


