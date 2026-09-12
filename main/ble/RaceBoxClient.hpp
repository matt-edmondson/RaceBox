// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "RaceBoxMessages.hpp"
#include "RaceboxData.hpp"
#include "UbxParser.hpp"

#include <cstdint>
#include <functional>

#if __has_include("host/ble_hs.h")
  #define RACEBOX_HAVE_NIMBLE 1
  #include "host/ble_gap.h"
  #include "host/ble_gatt.h"
  #include "host/ble_hs.h"
#endif

namespace ktsu { namespace racebox { namespace ble {

// Observable link state, surfaced so the UI can tell the user what is happening
// instead of leaving a default-constructed telemetry frame on screen.
enum class ConnectionState {
  Idle,        // stack not started
  Scanning,    // looking for a RaceBox
  Connecting,  // candidate found, link being established
  Connected,   // connected, discovering services
  Streaming,   // notifications enabled; telemetry is flowing
  Disconnected // link dropped; a rescan has been started
};

const char* toString(ConnectionState state);

// NimBLE central that finds a RaceBox Mini, subscribes to its Nordic UART
// service, and decodes the UBX telemetry stream.
//
// Threading: NimBLE invokes every callback on its own host task. Both listeners
// are therefore called from that task, NOT from the caller of loop(). Consumers
// that touch non-thread-safe libraries (LVGL, for one) must marshal the data
// across to their own task rather than acting on it inline.
class RaceBoxClient {
 public:
  using TelemetryListener = std::function<void(const RaceboxData&)>;
  using StateListener = std::function<void(ConnectionState)>;
  using GnssConfigListener = std::function<void(const GnssConfig&)>;

  RaceBoxClient() = default;
  RaceBoxClient(const RaceBoxClient&) = delete;
  RaceBoxClient& operator=(const RaceBoxClient&) = delete;

  // Bring up the controller and NimBLE host, then scan. Returns false if the
  // stack could not be started, leaving the caller free to continue headless.
  bool begin();

  // Present for symmetry with the main loop; NimBLE runs on its own task, so
  // there is nothing to pump here.
  void loop() {}

  void setTelemetryListener(TelemetryListener listener) {
    telemetryListener_ = std::move(listener);
  }
  void setStateListener(StateListener listener) { stateListener_ = std::move(listener); }
  // Called on the NimBLE host task when the device answers a configuration
  // query, which happens once per connection.
  void setGnssConfigListener(GnssConfigListener listener) {
    gnssConfigListener_ = std::move(listener);
  }

  ConnectionState state() const { return state_; }
  bool streaming() const { return state_ == ConnectionState::Streaming; }

  // Name of the peer we last connected to, empty until one is found.
  const char* peerName() const { return peerName_; }
  int8_t peerRssi() const { return peerRssi_; }
  // Derived from the advertised name; decides how telemetry byte 67 reads.
  DeviceModel deviceModel() const { return deviceModel_; }

  // Ask the device for its GNSS receiver configuration. Read-only: the device
  // replies with a 0xFF 0x27 message of its own, or a NACK on firmware older
  // than 3.3. Issued automatically once notifications are running, so the About
  // screen can show what the receiver is actually configured for.
  bool requestGnssConfig();

  // Last configuration the device reported, valid once hasGnssConfig() is true.
  bool hasGnssConfig() const { return hasGnssConfig_; }
  const GnssConfig& gnssConfig() const { return gnssConfig_; }

  // Frame and transmit a UBX packet on the UART RX characteristic. Returns
  // false when not connected, or when the framed packet exceeds what a single
  // ATT write can carry on the negotiated MTU.
  bool sendUbx(uint8_t msgClass, uint8_t msgId, const uint8_t* payload, uint16_t payloadLen);

  // Parser diagnostics, useful on an About/diagnostics screen.
  const UbxParser& parser() const { return parser_; }

 private:
  void setState(ConnectionState next);
  void startScan();
  void onNotifyData(const uint8_t* data, uint16_t len);
  // Acknowledgements and command replies arrive here from the parser.
  void onDeviceMessage(uint8_t msgClass, uint8_t msgId, const uint8_t* payload, size_t payloadLen);

#ifdef RACEBOX_HAVE_NIMBLE
  // NimBLE gives every GAP/GATT callback a user argument, so the instance is
  // threaded through explicitly rather than reached via a global.
  static int gapEventCb(struct ble_gap_event* event, void* arg);
  static int serviceDiscCb(uint16_t connHandle, const struct ble_gatt_error* error,
                           const struct ble_gatt_svc* service, void* arg);
  static int charDiscCb(uint16_t connHandle, const struct ble_gatt_error* error,
                        const struct ble_gatt_chr* chr, void* arg);
  static int descriptorDiscCb(uint16_t connHandle, const struct ble_gatt_error* error,
                              uint16_t chrDefHandle, const struct ble_gatt_dsc* dsc, void* arg);
  static int writeStatusCb(uint16_t connHandle, const struct ble_gatt_error* error,
                           struct ble_gatt_attr* attr, void* arg);
  // Outbound UBX writes need their own completion hook: writeStatusCb reports
  // the CCCD write and promotes the link to Streaming, which a command write
  // must not do.
  static int txStatusCb(uint16_t connHandle, const struct ble_gatt_error* error,
                        struct ble_gatt_attr* attr, void* arg);
  static int mtuCb(uint16_t connHandle, const struct ble_gatt_error* error, uint16_t mtu,
                   void* arg);

  // ble_hs_cfg's sync/reset hooks take no user argument, so these two -- and
  // only these two -- have to reach the instance through a static. Both check
  // it before use.
  static void onHostSync();
  static void onHostReset(int reason);
  static void hostTask(void* param);
  static RaceBoxClient* s_instance;

  bool matchesRacebox(const struct ble_gap_disc_desc& desc);
  // Drop a peer we cannot stream from -- a missing UART service, characteristic
  // or CCCD. Terminating the link routes recovery through the existing
  // disconnect handler, which rescans, instead of parking in Connected forever.
  void abandonConnection(const char* why);
  // Clear every per-connection handle so a stale value from a previous peer
  // cannot be mistaken for a successful discovery.
  void clearConnectionState();
#endif

  TelemetryListener telemetryListener_;
  StateListener stateListener_;
  GnssConfigListener gnssConfigListener_;
  UbxParser parser_;

  bool started_ = false;
  ConnectionState state_ = ConnectionState::Idle;

  uint8_t ownAddrType_ = 0;
  uint16_t connHandle_ = 0;
  uint16_t attMtu_ = 23; // ATT default until an exchange succeeds
  uint16_t uartSvcStart_ = 0;
  uint16_t uartSvcEnd_ = 0;
  uint16_t uartTxValHandle_ = 0; // notify (device -> us)
  uint16_t uartRxValHandle_ = 0; // write  (us -> device)
  bool cccdWriteStarted_ = false;
  bool gnssConfigRequested_ = false; // one query per connection
  bool hasGnssConfig_ = false;
  GnssConfig gnssConfig_;

  char peerName_[32] = {0};
  int8_t peerRssi_ = 0;
  DeviceModel deviceModel_ = DeviceModel::Unknown;
};

} } } // namespaces
