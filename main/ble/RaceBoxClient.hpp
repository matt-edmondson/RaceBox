// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <functional>
#include <cstdint>
#include <vector>

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

    // --- BLE / NimBLE state ---
    bool started_ = false;
    uint16_t connHandle_ = 0;
    uint16_t uartSvcStart_ = 0;
    uint16_t uartSvcEnd_ = 0;
    uint16_t uartTxValHandle_ = 0; // Notify from device
    uint16_t uartRxValHandle_ = 0; // Write to device

    // Buffer for reassembling UBX packets that may be split across notifications
    std::vector<uint8_t> notifyBuffer_{};

    // Internal helpers
    void handleNotifyData(const uint8_t* data, uint16_t len);
    void processUbxBuffer();
    // Callback friends declared in cpp with NimBLE headers; avoid header-level NimBLE types
    friend int gap_scan_cb(struct ble_gap_event* event, void* arg);
    friend int gap_conn_cb(struct ble_gap_event* event, void* arg);
};

} } } // namespaces


