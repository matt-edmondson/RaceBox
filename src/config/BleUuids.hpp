// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once
// This header intentionally avoids Arduino/NimBLE includes

namespace ktsu { namespace racebox { namespace ble {

// BLE UUIDs used by RaceBox
struct Uuids {
  // Device Information Service (DIS)
  static constexpr const char* deviceInfoService = "0000180a-0000-1000-8000-00805f9b34fb";
  static constexpr const char* modelCharacteristic = "00002a24-0000-1000-8000-00805f9b34fb";
  static constexpr const char* serialNumberCharacteristic = "00002a25-0000-1000-8000-00805f9b34fb";
  static constexpr const char* firmwareRevisionCharacteristic = "00002a26-0000-1000-8000-00805f9b34fb";
  static constexpr const char* hardwareRevisionCharacteristic = "00002a27-0000-1000-8000-00805f9b34fb";
  static constexpr const char* manufacturerCharacteristic = "00002a29-0000-1000-8000-00805f9b34fb";

  // Nordic UART Service (NUS)
  static constexpr const char* uartService = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
  static constexpr const char* uartRxCharacteristic = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; // Write
  static constexpr const char* uartTxCharacteristic = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // Notify

  // NMEA UART Service
  static constexpr const char* nmeaUartService = "00001101-0000-1000-8000-00805F9B34FB";
  static constexpr const char* nmeaRxCharacteristic = "00001102-0000-1000-8000-00805F9B34FB"; // Write
  static constexpr const char* nmeaTxCharacteristic = "00001103-0000-1000-8000-00805F9B34FB"; // Notify
};

} } }  // namespaces
