// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "RaceBoxClient.hpp"

#include "../common/IdfCompat.hpp"

using ktsu::racebox::ble::RaceBoxClient;

static const char* TAG = "RaceBoxClient";

void RaceBoxClient::begin() {
  ESP_LOGI(TAG, "BLE client init (stub)");
  // TODO: Implement NimBLE (IDF) central client for RaceBox Mini UART service
}

void RaceBoxClient::loop() {
  // TODO: Scan/connect/subscribe using NimBLE host in IDF
  // For now, nothing.
}


