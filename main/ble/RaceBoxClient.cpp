// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "RaceBoxClient.hpp"

#include "../common/IdfCompat.hpp"
#include <algorithm>
#include <cstring>
#if __has_include("esp_bt.h")
#include "esp_bt.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "../config/BleUuids.hpp"
#endif

using ktsu::racebox::ble::RaceBoxClient;

static const char* TAG = "RaceBoxClient";

#if __has_include("esp_bt.h")
namespace {
  // Forward declarations of callbacks
  int gap_scan_cb(struct ble_gap_event* event, void* arg);
  int gap_conn_cb(struct ble_gap_event* event, void* arg);
  int gatt_disc_svc_cb(uint16_t conn_handle, const ble_gatt_error* error, const ble_gatt_svc* service, void* arg);
  int gatt_disc_chr_cb(uint16_t conn_handle, const ble_gatt_error* error, const ble_gatt_chr* chr, void* arg);
  int gatt_disc_dsc_cb(uint16_t conn_handle, const ble_gatt_error* error, uint16_t chr_def_handle, const ble_gatt_dsc* dsc, void* arg);

  // Host task
  void host_task(void* param) { nimble_port_run(); nimble_port_freertos_deinit(); }

  // Singleton pointer to access instance inside static callbacks
  static RaceBoxClient* g_client_instance = nullptr;
}
#endif

void RaceBoxClient::begin() {
  ESP_LOGI(TAG, "BLE client init");
#if __has_include("esp_bt.h")
  if (started_) return;
  started_ = true;
  #if __has_include("esp_bt.h")
  // Save instance pointer for callbacks
  ::g_client_instance = this;
  #endif
  // Enable BLE controller and NimBLE host
  esp_err_t err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  if (err != ESP_OK) { ESP_LOGW(TAG, "esp_bt_controller_mem_release failed: %d", err); }
  ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());
  nimble_port_init();
  ble_svc_gap_init();
  ble_svc_gatt_init();
  ble_hs_cfg.reset_cb = [](int reason){ ESP_LOGW(TAG, "BLE reset, reason=%d", reason); };
  ble_hs_cfg.sync_cb = [](){
    ESP_LOGI(TAG, "BLE synced; starting scan");
    ble_svc_gap_device_name_set("RaceBoxClient");
    ble_gap_disc_params params{}; params.itvl=0x0060; params.window=0x0030; params.filter_policy=0; params.limited=0; params.passive=1;
    ble_gap_disc(0, BLE_HS_FOREVER, &params, gap_scan_cb, nullptr);
  };
  nimble_port_freertos_init(host_task);
#else
  ESP_LOGW(TAG, "Built without ESP-IDF BLE headers; running in stub mode");
#endif
}

void RaceBoxClient::loop() {
  // No-op; NimBLE runs in host task
}

void RaceBoxClient::handleNotifyData(const uint8_t* data, uint16_t len) {
  if (!data || len == 0) return;
  notifyBuffer_.insert(notifyBuffer_.end(), data, data + len);
  processUbxBuffer();
}

static bool ubx_validate_and_extract(const std::vector<uint8_t>& buf, size_t& out_packet_len) {
  out_packet_len = 0;
  if (buf.size() < 8) return false;
  size_t i = 0;
  while (i + 8 <= buf.size()) {
    if (buf[i] == 0xB5 && buf[i+1] == 0x62) break; i++;
  }
  if (i > 0) return false; // caller ensures buffer starts at header
  uint16_t payload_len = static_cast<uint16_t>(buf[4] | (buf[5] << 8));
  size_t pkt_len = 6 + payload_len + 2;
  if (buf.size() < pkt_len) return false;
  uint8_t ck_a = 0, ck_b = 0; for (size_t j = 2; j < 6 + payload_len; ++j) { ck_a = ck_a + buf[j]; ck_b = ck_b + ck_a; }
  if (buf[6 + payload_len] != ck_a || buf[7 + payload_len] != ck_b) { out_packet_len = pkt_len; return true; } // invalid, but consume to resync
  out_packet_len = pkt_len; return true;
}

void RaceBoxClient::processUbxBuffer() {
  // Find frames; consume as they complete
  for (;;) {
    if (notifyBuffer_.size() < 8) return;
    // Align to header
    const uint8_t hdr[2] = {0xB5, 0x62};
    auto it = std::search(notifyBuffer_.begin(), notifyBuffer_.end(), std::begin(hdr), std::end(hdr));
    if (it != notifyBuffer_.begin()) {
      if (it == notifyBuffer_.end()) { notifyBuffer_.clear(); return; }
      notifyBuffer_.erase(notifyBuffer_.begin(), it);
      if (notifyBuffer_.size() < 8) return;
    }
    size_t pkt_len = 0; if (!ubx_validate_and_extract(notifyBuffer_, pkt_len)) return;
    if (pkt_len == 0 || notifyBuffer_.size() < pkt_len) return;
    // Parse if class=0xFF id=0x01 and payload length matches 80
    const uint8_t* pkt = notifyBuffer_.data();
    uint8_t msg_class = pkt[2]; uint8_t msg_id = pkt[3]; uint16_t payload_len = static_cast<uint16_t>(pkt[4] | (pkt[5] << 8));
    if (msg_class == 0xFF && msg_id == 0x01 && payload_len >= 80) {
      const uint8_t* p = pkt + 6;
      RaceboxData d{};
      // sats at offset 23 (byte)
      d.sats = p[23];
      // MSL altitude at offset 36 (Int32, mm) -> meters
      int32_t alt_mm = static_cast<int32_t>(p[36] | (p[37] << 8) | (p[38] << 16) | (p[39] << 24));
      d.altitudeM = static_cast<float>(alt_mm) / 1000.0f;
      // Speed at offset 48 (Int32, mm/s) -> km/h
      int32_t spd_mms = static_cast<int32_t>(p[48] | (p[49] << 8) | (p[50] << 16) | (p[51] << 24));
      d.speedKmh = static_cast<float>(spd_mms) * 3.6f / 1000.0f;
      if (telemetryListener_) telemetryListener_(d);
    }
    // Consume packet
    notifyBuffer_.erase(notifyBuffer_.begin(), notifyBuffer_.begin() + pkt_len);
  }
}

#if __has_include("esp_bt.h")
// Static callback implementations
int gap_scan_cb(struct ble_gap_event* ev, void* arg) {
  (void)arg;
  if (ev->type == BLE_GAP_EVENT_DISC) {
    const ble_gap_disc_desc& d = ev->disc;
    ble_hs_adv_fields f{};
    if (ble_hs_adv_parse_fields(&f, d.data, d.length_data) == 0) {
      bool name_ok = false;
      if (f.name && f.name_len) {
        const char* n = reinterpret_cast<const char*>(f.name);
        name_ok = (f.name_len >= 8 && strncmp(n, "RaceBox ", 8) == 0) || (f.name_len >= 12 && strncmp(n, "RaceBox Mini", 12) == 0);
      }
      bool svc_ok = false;
      ble_uuid128_t uart_uuid{}; ble_uuid128_from_str(ktsu::racebox::ble::Uuids::uartService, &uart_uuid);
      for (int i = 0; i < f.num_uuids128; ++i) {
        if (ble_uuid_cmp(&f.uuids128[i].u, &uart_uuid.u) == 0) { svc_ok = true; break; }
      }
      if (name_ok || svc_ok) {
        ESP_LOGI(TAG, "Found candidate; connecting");
        ble_gap_disc_cancel();
        ble_gap_connect(0, &d.addr, BLE_HS_FOREVER, nullptr, gap_conn_cb, nullptr);
      }
    }
  }
  return 0;
}

int gap_conn_cb(struct ble_gap_event* ev, void* arg) {
  (void)arg;
  RaceBoxClient* self = ::g_client_instance;
  switch (ev->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (ev->connect.status == 0) {
        self->connHandle_ = ev->connect.conn_handle;
        ESP_LOGI(TAG, "Connected: handle=%u", self->connHandle_);
        ble_uuid128_t svc_uuid{}; ble_uuid128_from_str(ktsu::racebox::ble::Uuids::uartService, &svc_uuid);
        int rc = ble_gattc_disc_svc_by_uuid(self->connHandle_, &svc_uuid.u, gatt_disc_svc_cb, self);
        if (rc) ESP_LOGE(TAG, "disc_svc rc=%d", rc);
      } else {
        ESP_LOGE(TAG, "Connect failed: %d; restarting scan", ev->connect.status);
        ble_gap_disc_params p{}; p.itvl=0x0060; p.window=0x0030; p.passive=1; ble_gap_disc(0, BLE_HS_FOREVER, &p, gap_scan_cb, nullptr);
      }
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      ESP_LOGW(TAG, "Disconnected: reason=%d", ev->disconnect.reason);
      self->connHandle_ = 0; self->uartTxValHandle_ = 0; self->uartRxValHandle_ = 0; self->notifyBuffer_.clear();
      {
        ble_gap_disc_params p{}; p.itvl=0x0060; p.window=0x0030; p.passive=1; ble_gap_disc(0, BLE_HS_FOREVER, &p, gap_scan_cb, nullptr);
      }
      break;
    case BLE_GAP_EVENT_NOTIFY_RX:
      if (ev->notify_rx.attr_handle == self->uartTxValHandle_ && ev->notify_rx.om) {
        uint16_t len = OS_MBUF_PKTLEN(ev->notify_rx.om);
        std::vector<uint8_t> tmp(len);
        os_mbuf_copydata(ev->notify_rx.om, 0, len, tmp.data());
        self->handleNotifyData(tmp.data(), len);
      }
      break;
    default: break;
  }
  return 0;
}

int gatt_disc_svc_cb(uint16_t conn_handle, const ble_gatt_error* error, const ble_gatt_svc* service, void* arg) {
  auto* self = static_cast<RaceBoxClient*>(arg);
  if (error->status != 0) { ESP_LOGE(TAG, "Service discovery error: %d", error->status); return BLE_HS_EDONE; }
  self->uartSvcStart_ = service->start_handle; self->uartSvcEnd_ = service->end_handle;
  ESP_LOGI(TAG, "UART svc: %u..%u", self->uartSvcStart_, self->uartSvcEnd_);
  return ble_gattc_disc_all_chrs(conn_handle, self->uartSvcStart_, self->uartSvcEnd_, gatt_disc_chr_cb, self);
}

int gatt_disc_chr_cb(uint16_t conn_handle, const ble_gatt_error* ch_err, const ble_gatt_chr* chr, void* arg) {
  auto* self = static_cast<RaceBoxClient*>(arg);
  if (ch_err->status == BLE_HS_EDONE) {
    // Done discovering chars. If we have TX, discover descriptors to find CCCD
    if (self->uartTxValHandle_) {
      return ble_gattc_disc_all_dscs(conn_handle, self->uartTxValHandle_, self->uartSvcEnd_, gatt_disc_dsc_cb, self);
    }
    return BLE_HS_EDONE;
  }
  if (ch_err->status != 0) { ESP_LOGE(TAG, "Char discovery error: %d", ch_err->status); return ch_err->status; }
  ble_uuid128_t tx_uuid{}; ble_uuid128_from_str(ktsu::racebox::ble::Uuids::uartTxCharacteristic, &tx_uuid);
  ble_uuid128_t rx_uuid{}; ble_uuid128_from_str(ktsu::racebox::ble::Uuids::uartRxCharacteristic, &rx_uuid);
  if (ble_uuid_cmp(&chr->uuid.u, &tx_uuid.u) == 0) { self->uartTxValHandle_ = chr->val_handle; ESP_LOGI(TAG, "TX handle=%u", self->uartTxValHandle_); }
  if (ble_uuid_cmp(&chr->uuid.u, &rx_uuid.u) == 0) { self->uartRxValHandle_ = chr->val_handle; ESP_LOGI(TAG, "RX handle=%u", self->uartRxValHandle_); }
  return 0;
}

int gatt_disc_dsc_cb(uint16_t conn_handle, const ble_gatt_error* error, uint16_t chr_def_handle, const ble_gatt_dsc* dsc, void* arg) {
  auto* self = static_cast<RaceBoxClient*>(arg);
  (void)chr_def_handle;
  if (error->status == BLE_HS_EDONE) { return BLE_HS_EDONE; }
  if (error->status != 0) { ESP_LOGE(TAG, "Desc discovery error: %d", error->status); return error->status; }
  // Look for CCCD (0x2902)
  ble_uuid16_t cccd_uuid{}; cccd_uuid.u.type = BLE_UUID_TYPE_16; cccd_uuid.value = 0x2902;
  if (ble_uuid_cmp(&dsc->uuid.u, &cccd_uuid.u) == 0) {
    const uint8_t en[2] = {0x01, 0x00};
    int rc = ble_gattc_write_flat(conn_handle, dsc->handle, en, sizeof(en), [](uint16_t, const ble_gatt_error* we, uint16_t, void* a){
      if (we->status) ESP_LOGE(TAG, "Enable notify failed: %d", we->status); else ESP_LOGI(TAG, "Notifications enabled");
      return 0;
    }, self);
    if (rc) ESP_LOGE(TAG, "CCCD write rc=%d", rc);
  }
  return 0;
}
#endif


