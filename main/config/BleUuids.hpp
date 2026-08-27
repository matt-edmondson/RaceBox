// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

namespace ktsu { namespace racebox { namespace ble {

// UUIDs used to find and talk to a RaceBox Mini.
//
// The UART service below is the standard Nordic UART Service (NUS), which is
// what RaceBox devices expose for their binary UBX stream. These values are
// used directly by service discovery in RaceBoxClient -- they are not
// placeholders and should not need editing for a Mini / Mini S.
//
// Note: earlier revisions of this file also listed a secondary "NMEA UART"
// service using 00001101-.... That is the Bluetooth *Classic* Serial Port
// Profile UUID, not a BLE GATT service, and it was never verified against a
// device. It has been removed rather than left to mislead. If a secondary NMEA
// service is needed for the Mini S or Micro, take the UUIDs from the official
// RaceBox BLE protocol documentation and add them here with a comment naming
// the document revision they came from.
struct Uuids {
  // Standard GATT Device Information service (0x180A) and its characteristics.
  static constexpr const char* deviceInfoService = "0000180a-0000-1000-8000-00805f9b34fb";
  static constexpr const char* modelCharacteristic = "00002a24-0000-1000-8000-00805f9b34fb";
  static constexpr const char* serialNumberCharacteristic = "00002a25-0000-1000-8000-00805f9b34fb";
  static constexpr const char* firmwareRevisionCharacteristic = "00002a26-0000-1000-8000-00805f9b34fb";
  static constexpr const char* hardwareRevisionCharacteristic = "00002a27-0000-1000-8000-00805f9b34fb";
  static constexpr const char* manufacturerCharacteristic = "00002a29-0000-1000-8000-00805f9b34fb";

  // Nordic UART Service: the UBX telemetry transport.
  static constexpr const char* uartService = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
  static constexpr const char* uartRxCharacteristic = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"; // write  (host -> device)
  static constexpr const char* uartTxCharacteristic = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"; // notify (device -> host)
};

} } } // namespaces
