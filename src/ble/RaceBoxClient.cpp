// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "RaceBoxClient.hpp"
#include "../config/BleUuids.hpp"

#if __has_include(<NimBLEDevice.h>)
#  include <Arduino.h>
#  include <NimBLEDevice.h>
#  define HAS_NIMBLE 1
#else
#  define HAS_NIMBLE 0
#endif

using namespace ktsu::racebox::ble;

static const char* kDeviceNamePrefix = "RaceBox"; // adjust if needed
static RaceBoxClient* sInstance = nullptr;
static void NotifyThunk(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool isNotify) {
  if (sInstance) sInstance->handleNotify(chr, data, len, isNotify);
}

void RaceBoxClient::begin() {
#if HAS_NIMBLE
  NimBLEDevice::init("RaceBox-UI");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
#endif
  sInstance = this;
}

void RaceBoxClient::loop() { connectIfNeeded(); }

void RaceBoxClient::connectIfNeeded() {
#if HAS_NIMBLE
  if (client_ && client_->isConnected()) return;

  uint32_t now = millis();
  if (now - lastReconnectAttemptMs_ < 3000) return;
  lastReconnectAttemptMs_ = now;

  if (!client_) { client_ = NimBLEDevice::createClient(); }

  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(45);
  scan->setWindow(30);
  NimBLEScanResults results = scan->start(3, false);
  NimBLEAdvertisedDevice target;
  bool found = false;
  for (int i = 0; i < results.getCount(); ++i) {
    NimBLEAdvertisedDevice dev = results.getDevice(i);
    if (dev.getName().find(kDeviceNamePrefix) == 0) { target = dev; found = true; break; }
  }
  if (!found) return;

  if (!client_->connect(target.getAddress())) { client_->disconnect(); return; }

  // Try NMEA UART service first, then Nordic UART service as fallback
  NimBLERemoteService* svc = client_->getService(Uuids::nmeaUartService);
  if (!svc) svc = client_->getService(Uuids::uartService);
  if (!svc) { client_->disconnect(); return; }

  // Prefer NMEA TX/RX when available, else use NUS TX/RX
  txChr_ = svc->getCharacteristic(Uuids::nmeaTxCharacteristic);
  rxChr_ = svc->getCharacteristic(Uuids::nmeaRxCharacteristic);
  if (!txChr_ || !rxChr_) {
    txChr_ = svc->getCharacteristic(Uuids::uartTxCharacteristic);
    rxChr_ = svc->getCharacteristic(Uuids::uartRxCharacteristic);
  }
  if (!txChr_ || !rxChr_) { client_->disconnect(); return; }

  if (txChr_->canNotify()) { txChr_->subscribe(true, NotifyThunk); }
#else
  (void)kDeviceNamePrefix;
  return;
#endif
}

void RaceBoxClient::handleNotify(NimBLERemoteCharacteristic* chr, uint8_t* data, size_t len, bool /*isNotify*/) {
  // Placeholder: parse according to protocol rev 8
  RaceboxData d{};
  if (len >= 8) {
    // Fake decode for now
    d.speedKmh = data[0];
    d.altitudeM = data[1];
    d.sats = data[2];
  }
  if (telemetryListener_) telemetryListener_(d);
}

#if !HAS_NIMBLE
// Provide no-op definitions when NimBLE is unavailable to satisfy tooling
// (Runtime behavior requires NimBLE to function.)
// The above methods already compiled; additional stubs not needed.
#endif


