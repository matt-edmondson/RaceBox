// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "RaceBoxClient.hpp"

#include "../common/IdfCompat.hpp"
#include "../config/BleUuids.hpp"

#include <cstring>
#include <vector>

#ifdef RACEBOX_HAVE_NIMBLE
  #include "esp_bt.h"
  #include "nimble/nimble_port.h"
  #include "nimble/nimble_port_freertos.h"
  #include "host/ble_hs.h"
  #include "host/ble_uuid.h"
  #include "host/util/util.h"
  #include "services/gap/ble_svc_gap.h"
  #include "services/gatt/ble_svc_gatt.h"
#endif

namespace ktsu { namespace racebox { namespace ble {

namespace {
constexpr const char* TAG = "RaceBoxClient";

#ifdef RACEBOX_HAVE_NIMBLE
// ESP-IDF's NimBLE exposes no ble_uuid128_from_str(), so UUIDs are handed over
// as little-endian bytes derived at compile time in BleUuids.hpp.
ble_uuid128_t makeUuid128(const ktsu::racebox::config::Uuid128Bytes& src) {
  ble_uuid128_t uuid{};
  uuid.u.type = BLE_UUID_TYPE_128;
  memcpy(uuid.value, src.bytes, sizeof(uuid.value));
  return uuid;
}
#endif
constexpr uint16_t kCccdUuid = 0x2902;
constexpr uint16_t kPreferredMtu = 247; // enough for a whole RaceBox frame
} // namespace

const char* toString(ConnectionState state) {
  switch (state) {
    case ConnectionState::Idle: return "Idle";
    case ConnectionState::Scanning: return "Scanning";
    case ConnectionState::Connecting: return "Connecting";
    case ConnectionState::Connected: return "Connected";
    case ConnectionState::Streaming: return "Streaming";
    case ConnectionState::Disconnected: return "Disconnected";
  }
  return "Unknown";
}

void RaceBoxClient::setState(ConnectionState next) {
  if (state_ == next) return;
  state_ = next;
  ESP_LOGI(TAG, "state -> %s", toString(next));
  if (stateListener_) stateListener_(next);
}

void RaceBoxClient::onNotifyData(const uint8_t* data, uint16_t len) {
  parser_.append(data, len);
}

#ifdef RACEBOX_HAVE_NIMBLE

RaceBoxClient* RaceBoxClient::s_instance = nullptr;

void RaceBoxClient::hostTask(void* param) {
  (void)param;
  nimble_port_run();
  nimble_port_freertos_deinit();
}

void RaceBoxClient::onHostReset(int reason) {
  ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
  if (s_instance) s_instance->setState(ConnectionState::Disconnected);
}

void RaceBoxClient::onHostSync() {
  if (!s_instance) {
    ESP_LOGE(TAG, "host synced with no client instance");
    return;
  }
  // Make sure we have a usable identity address, then ask the host which type
  // to advertise -- boards without a burned-in public address must use a
  // random static one, and hardcoding public would fail on those.
  int rc = ble_hs_util_ensure_addr(0);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed: %d", rc);
    return;
  }
  rc = ble_hs_id_infer_auto(0, &s_instance->ownAddrType_);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
    return;
  }
  ble_svc_gap_device_name_set("RaceBoxClient");
  s_instance->startScan();
}

void RaceBoxClient::clearConnectionState() {
  connHandle_ = 0;
  attMtu_ = 23;
  uartSvcStart_ = 0;
  uartSvcEnd_ = 0;
  uartTxValHandle_ = 0;
  uartRxValHandle_ = 0;
  cccdWriteStarted_ = false;
  parser_.reset();
}

void RaceBoxClient::abandonConnection(const char* why) {
  ESP_LOGE(TAG, "unusable peer (%s); dropping the link", why);
  // The disconnect event does the cleanup and starts the next scan. If the
  // terminate itself fails there is no link left to wait on, so recover here.
  const int rc = ble_gap_terminate(connHandle_, BLE_ERR_REM_USER_CONN_TERM);
  if (rc != 0) {
    ESP_LOGW(TAG, "ble_gap_terminate failed: %d; rescanning anyway", rc);
    clearConnectionState();
    setState(ConnectionState::Disconnected);
    startScan();
  }
}

void RaceBoxClient::startScan() {
  ble_gap_disc_params params{};
  params.itvl = 0x0060;
  params.window = 0x0030;
  params.filter_policy = 0;
  params.limited = 0;
  params.passive = 0; // active scan, so we receive scan-response names
  params.filter_duplicates = 1;

  const int rc = ble_gap_disc(ownAddrType_, BLE_HS_FOREVER, &params, &RaceBoxClient::gapEventCb,
                              this);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gap_disc failed: %d", rc);
    return;
  }
  setState(ConnectionState::Scanning);
}

bool RaceBoxClient::matchesRacebox(const struct ble_gap_disc_desc& desc) {
  ble_hs_adv_fields fields{};
  if (ble_hs_adv_parse_fields(&fields, desc.data, desc.length_data) != 0) return false;

  bool nameMatch = false;
  if (fields.name && fields.name_len > 0) {
    const size_t n = fields.name_len < sizeof(peerName_) - 1 ? fields.name_len
                                                            : sizeof(peerName_) - 1;
    // RaceBox devices advertise as "RaceBox <model> <serial>".
    if (fields.name_len >= 7 &&
        strncmp(reinterpret_cast<const char*>(fields.name), "RaceBox", 7) == 0) {
      nameMatch = true;
      memcpy(peerName_, fields.name, n);
      peerName_[n] = '\0';
    }
  }

  bool serviceMatch = false;
  const ble_uuid128_t uartUuid = makeUuid128(kUartServiceBytes);
  for (int i = 0; i < fields.num_uuids128; ++i) {
    if (ble_uuid_cmp(&fields.uuids128[i].u, &uartUuid.u) == 0) {
      serviceMatch = true;
      break;
    }
  }
  return nameMatch || serviceMatch;
}

int RaceBoxClient::gapEventCb(struct ble_gap_event* ev, void* arg) {
  auto* self = static_cast<RaceBoxClient*>(arg);
  if (!self || !ev) return 0;

  switch (ev->type) {
    case BLE_GAP_EVENT_DISC: {
      if (!self->matchesRacebox(ev->disc)) return 0;
      self->peerRssi_ = ev->disc.rssi;
      ESP_LOGI(TAG, "found '%s' (rssi %d); connecting",
               self->peerName_[0] ? self->peerName_ : "RaceBox", self->peerRssi_);
      ble_gap_disc_cancel();
      self->setState(ConnectionState::Connecting);
      const int rc = ble_gap_connect(self->ownAddrType_, &ev->disc.addr, 30000, nullptr,
                                     &RaceBoxClient::gapEventCb, self);
      if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_connect failed: %d", rc);
        self->startScan();
      }
      return 0;
    }

    case BLE_GAP_EVENT_CONNECT: {
      if (ev->connect.status != 0) {
        ESP_LOGE(TAG, "connect failed: %d; rescanning", ev->connect.status);
        self->startScan();
        return 0;
      }
      self->clearConnectionState();
      self->connHandle_ = ev->connect.conn_handle;
      self->setState(ConnectionState::Connected);

      // A larger MTU lets a whole 88-byte RaceBox frame arrive in one
      // notification instead of four.
      ble_att_set_preferred_mtu(kPreferredMtu);
      ble_gattc_exchange_mtu(self->connHandle_, &RaceBoxClient::mtuCb, self);

      const ble_uuid128_t svcUuid = makeUuid128(kUartServiceBytes);
      const int rc = ble_gattc_disc_svc_by_uuid(self->connHandle_, &svcUuid.u,
                                                &RaceBoxClient::serviceDiscCb, self);
      if (rc != 0) self->abandonConnection("service discovery could not be started");
      return 0;
    }

    case BLE_GAP_EVENT_DISCONNECT: {
      ESP_LOGW(TAG, "disconnected: reason=%d", ev->disconnect.reason);
      self->clearConnectionState();
      self->setState(ConnectionState::Disconnected);
      self->startScan();
      return 0;
    }

    case BLE_GAP_EVENT_NOTIFY_RX: {
      if (ev->notify_rx.attr_handle != self->uartTxValHandle_ || !ev->notify_rx.om) return 0;
      const uint16_t len = OS_MBUF_PKTLEN(ev->notify_rx.om);
      if (len == 0) return 0;
      // Stack buffer sized for a full ATT payload avoids heap churn at 25 Hz.
      // A negotiated MTU cannot exceed kPreferredMtu, so this only guards
      // against a stack that hands us more than it agreed to: appending a
      // truncated notification would silently corrupt the frame stream.
      uint8_t chunk[kPreferredMtu];
      if (static_cast<size_t>(len) > sizeof(chunk)) {
        ESP_LOGE(TAG, "notification of %u bytes exceeds the %u-byte buffer; resyncing",
                 static_cast<unsigned>(len), static_cast<unsigned>(sizeof(chunk)));
        self->parser_.reset();
        return 0;
      }
      if (os_mbuf_copydata(ev->notify_rx.om, 0, len, chunk) == 0) {
        self->onNotifyData(chunk, len);
      }
      return 0;
    }

    case BLE_GAP_EVENT_MTU: {
      self->attMtu_ = ev->mtu.value;
      ESP_LOGI(TAG, "MTU now %u", self->attMtu_);
      return 0;
    }

    default:
      return 0;
  }
}

int RaceBoxClient::mtuCb(uint16_t connHandle, const struct ble_gatt_error* error, uint16_t mtu,
                         void* arg) {
  (void)connHandle;
  auto* self = static_cast<RaceBoxClient*>(arg);
  if (!self) return 0;
  if (error && error->status == 0) {
    self->attMtu_ = mtu;
    ESP_LOGI(TAG, "MTU exchange ok: %u", mtu);
  } else {
    ESP_LOGW(TAG, "MTU exchange failed (%d); staying at %u",
             error ? error->status : -1, self->attMtu_);
  }
  return 0;
}

int RaceBoxClient::serviceDiscCb(uint16_t connHandle, const struct ble_gatt_error* error,
                                 const struct ble_gatt_svc* service, void* arg) {
  auto* self = static_cast<RaceBoxClient*>(arg);
  if (!self) return BLE_HS_EDONE;

  if (error && error->status == BLE_HS_EDONE) {
    if (self->uartSvcStart_ == 0) self->abandonConnection("no UART service");
    return 0;
  }
  if (!error || error->status != 0) {
    ESP_LOGE(TAG, "service discovery error: %d", error ? error->status : -1);
    return error ? error->status : BLE_HS_EDONE;
  }
  if (!service) return 0;

  self->uartSvcStart_ = service->start_handle;
  self->uartSvcEnd_ = service->end_handle;
  ESP_LOGI(TAG, "UART service handles %u..%u", self->uartSvcStart_, self->uartSvcEnd_);

  return ble_gattc_disc_all_chrs(connHandle, self->uartSvcStart_, self->uartSvcEnd_,
                                 &RaceBoxClient::charDiscCb, self);
}

int RaceBoxClient::charDiscCb(uint16_t connHandle, const struct ble_gatt_error* error,
                              const struct ble_gatt_chr* chr, void* arg) {
  auto* self = static_cast<RaceBoxClient*>(arg);
  if (!self) return BLE_HS_EDONE;

  if (error && error->status == BLE_HS_EDONE) {
    if (self->uartTxValHandle_ == 0) {
      self->abandonConnection("no UART TX characteristic");
      return 0;
    }
    // Find the CCCD that turns notifications on.
    return ble_gattc_disc_all_dscs(connHandle, self->uartTxValHandle_, self->uartSvcEnd_,
                                   &RaceBoxClient::descriptorDiscCb, self);
  }
  if (!error || error->status != 0) {
    ESP_LOGE(TAG, "characteristic discovery error: %d", error ? error->status : -1);
    return error ? error->status : BLE_HS_EDONE;
  }
  if (!chr) return 0;

  const ble_uuid128_t txUuid = makeUuid128(kUartTxBytes);
  const ble_uuid128_t rxUuid = makeUuid128(kUartRxBytes);

  if (ble_uuid_cmp(&chr->uuid.u, &txUuid.u) == 0) {
    self->uartTxValHandle_ = chr->val_handle;
    ESP_LOGI(TAG, "TX (notify) handle=%u", self->uartTxValHandle_);
  } else if (ble_uuid_cmp(&chr->uuid.u, &rxUuid.u) == 0) {
    self->uartRxValHandle_ = chr->val_handle;
    ESP_LOGI(TAG, "RX (write) handle=%u", self->uartRxValHandle_);
  }
  return 0;
}

int RaceBoxClient::descriptorDiscCb(uint16_t connHandle, const struct ble_gatt_error* error,
                                    uint16_t chrDefHandle, const struct ble_gatt_dsc* dsc,
                                    void* arg) {
  (void)chrDefHandle;
  auto* self = static_cast<RaceBoxClient*>(arg);
  if (!self) return BLE_HS_EDONE;

  if (error && error->status == BLE_HS_EDONE) {
    if (!self->cccdWriteStarted_) self->abandonConnection("no CCCD on the TX characteristic");
    return 0;
  }
  if (!error || error->status != 0) {
    ESP_LOGE(TAG, "descriptor discovery error: %d", error ? error->status : -1);
    return error ? error->status : BLE_HS_EDONE;
  }
  if (!dsc) return 0;

  ble_uuid16_t cccd{};
  cccd.u.type = BLE_UUID_TYPE_16;
  cccd.value = kCccdUuid;
  if (ble_uuid_cmp(&dsc->uuid.u, &cccd.u) != 0) return 0;

  const uint8_t enableNotify[2] = {0x01, 0x00};
  const int rc = ble_gattc_write_flat(connHandle, dsc->handle, enableNotify,
                                      sizeof(enableNotify), &RaceBoxClient::writeStatusCb, self);
  if (rc != 0) {
    self->abandonConnection("CCCD write could not be started");
    return 0;
  }
  self->cccdWriteStarted_ = true;
  return 0;
}

int RaceBoxClient::writeStatusCb(uint16_t connHandle, const struct ble_gatt_error* error,
                                 struct ble_gatt_attr* attr, void* arg) {
  (void)connHandle;
  (void)attr;
  auto* self = static_cast<RaceBoxClient*>(arg);
  if (!self) return 0;

  if (error && error->status == 0) {
    ESP_LOGI(TAG, "notifications enabled");
    self->setState(ConnectionState::Streaming);
  } else {
    ESP_LOGE(TAG, "enabling notifications failed: %d", error ? error->status : -1);
    self->abandonConnection("notifications could not be enabled");
  }
  return 0;
}

int RaceBoxClient::txStatusCb(uint16_t connHandle, const struct ble_gatt_error* error,
                              struct ble_gatt_attr* attr, void* arg) {
  (void)connHandle;
  (void)attr;
  (void)arg;
  if (error && error->status == 0) {
    ESP_LOGD(TAG, "UBX write acknowledged");
  } else {
    ESP_LOGW(TAG, "UBX write failed: %d", error ? error->status : -1);
  }
  return 0;
}

bool RaceBoxClient::begin() {
  if (started_) return true;

  s_instance = this;
  parser_.setSink([this](const RaceboxData& data) {
    if (telemetryListener_) telemetryListener_(data);
  });

  // ESP32-S3 has no Classic BT; reclaiming its memory is harmless elsewhere.
  const esp_err_t relErr = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  if (relErr != ESP_OK && relErr != ESP_ERR_INVALID_STATE) {
    ESP_LOGD(TAG, "esp_bt_controller_mem_release: %s", esp_err_to_name(relErr));
  }

  // Since ESP-IDF v5.0 nimble_port_init() also brings up the controller and HCI;
  // the old esp_nimble_hci_and_controller_init() no longer exists.
  const esp_err_t err = nimble_port_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(err));
    return false;
  }

  ble_svc_gap_init();
  ble_svc_gatt_init();
  ble_hs_cfg.reset_cb = &RaceBoxClient::onHostReset;
  ble_hs_cfg.sync_cb = &RaceBoxClient::onHostSync;

  nimble_port_freertos_init(&RaceBoxClient::hostTask);
  started_ = true;
  ESP_LOGI(TAG, "NimBLE central started");
  return true;
}

bool RaceBoxClient::sendUbx(uint8_t msgClass, uint8_t msgId, const uint8_t* payload,
                            uint16_t payloadLen) {
  if (state_ != ConnectionState::Streaming && state_ != ConnectionState::Connected) {
    ESP_LOGW(TAG, "sendUbx while not connected");
    return false;
  }
  if (uartRxValHandle_ == 0) {
    ESP_LOGW(TAG, "sendUbx before the RX characteristic was discovered");
    return false;
  }
  if (payloadLen > UbxParser::kMaxPayloadLen) {
    ESP_LOGE(TAG, "sendUbx payload too large: %u", payloadLen);
    return false;
  }

  const std::vector<uint8_t> frame = UbxParser::buildFrame(msgClass, msgId, payload, payloadLen);
  if (frame.empty()) {
    ESP_LOGE(TAG, "sendUbx could not frame the packet");
    return false;
  }

  // An ATT write carries at most MTU-3 bytes. Splitting a UBX packet across
  // writes is not attempted: every command we send is far smaller than this,
  // and a silently torn frame would be worse than a clear failure.
  const size_t maxWrite = attMtu_ > 3 ? static_cast<size_t>(attMtu_ - 3) : 20;
  if (frame.size() > maxWrite) {
    ESP_LOGE(TAG, "UBX frame of %u bytes exceeds the %u-byte ATT write limit",
             static_cast<unsigned>(frame.size()), static_cast<unsigned>(maxWrite));
    return false;
  }

  const int rc = ble_gattc_write_flat(connHandle_, uartRxValHandle_, frame.data(),
                                      static_cast<uint16_t>(frame.size()),
                                      &RaceBoxClient::txStatusCb, this);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gattc_write_flat failed: %d", rc);
    return false;
  }
  return true;
}

#else // !RACEBOX_HAVE_NIMBLE

// Host / lint build: no BLE stack available.
void RaceBoxClient::startScan() {}

bool RaceBoxClient::begin() {
  parser_.setSink([this](const RaceboxData& data) {
    if (telemetryListener_) telemetryListener_(data);
  });
  ESP_LOGW(TAG, "built without NimBLE headers; BLE disabled");
  return false;
}

bool RaceBoxClient::sendUbx(uint8_t, uint8_t, const uint8_t*, uint16_t) { return false; }

#endif // RACEBOX_HAVE_NIMBLE

} } } // namespaces
