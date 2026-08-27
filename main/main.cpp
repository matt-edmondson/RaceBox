// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "ble/RaceBoxClient.hpp"
#include "common/IdfCompat.hpp"
#include "config/Pins.hpp"
#include "config/Settings.hpp"
#include "io/EncoderInput.hpp"
#include "ui/Display.hpp"
#include "ui/LapTimer.hpp"

#if __has_include("nvs_flash.h")
  #include "nvs_flash.h"
  #define RACEBOX_HAVE_NVS_FLASH 1
#endif

using ktsu::racebox::ble::ConnectionState;
using ktsu::racebox::ble::RaceBoxClient;
using ktsu::racebox::ble::RaceboxData;
using ktsu::racebox::config::Settings;
using ktsu::racebox::io::EncoderEvent;
using ktsu::racebox::io::EncoderEventType;
using ktsu::racebox::io::EncoderInput;
using ktsu::racebox::ui::Display;
using ktsu::racebox::ui::LapTimer;

namespace {

constexpr const char* TAG = "app";

// LVGL's render pipeline needs considerably more stack than the default main
// task provides, so the UI runs on a task with an explicit allocation.
constexpr uint32_t kUiTaskStackBytes = 8192;
constexpr int kUiTaskPriority = 5;
constexpr int kUiTickMs = 5;

Settings g_settings;
LapTimer g_lapTimer;
Display g_display;
EncoderInput g_encoder(ktsu::Pins::encoderPinA, ktsu::Pins::encoderPinB,
                       ktsu::Pins::encoderButtonPin);
RaceBoxClient g_client;

// Runs on the UI task. Every LVGL call in the firmware originates here.
void onEncoderEvent(const EncoderEvent& event) {
  switch (event.type) {
    case EncoderEventType::Rotate: g_display.onRotate(event.delta); break;
    case EncoderEventType::Click: g_display.onClick(); break;
    case EncoderEventType::LongPress: g_display.onLongPress(); break;
  }
}

// Both of these run on the NimBLE host task; they only stash data for the UI
// task to pick up, and must never touch LVGL directly.
void onTelemetry(const RaceboxData& data) { g_display.updateTelemetry(data); }

// Reading the client's peer fields here is safe: they are written on this same
// NimBLE host task, immediately before the state change is published.
void onConnectionState(ConnectionState state) {
  g_display.updateLink(state, g_client.peerName(), g_client.peerRssi());
}

bool initNvs() {
#ifdef RACEBOX_HAVE_NVS_FLASH
  // The BLE controller keeps PHY calibration here and NimBLE stores bonding
  // data, so this has to succeed before the stack starts.
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS needs erasing (%s); reformatting", esp_err_to_name(err));
    if ((err = nvs_flash_erase()) == ESP_OK) err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
    return false;
  }
#endif
  return true;
}

void uiTask(void* param) {
  (void)param;
  for (;;) {
    g_encoder.tick();
    g_display.loop();
    vTaskDelay(pdMS_TO_TICKS(kUiTickMs));
  }
}

} // namespace

extern "C" void app_main(void) {
  ESP_LOGI(TAG, "RaceBox Mini Interface starting");

  const bool nvsOk = initNvs();
  if (nvsOk) g_settings.load();

  g_display.setSettings(&g_settings);
  g_display.setLapTimer(&g_lapTimer);
  if (!g_display.begin()) {
    // Keep going headless: telemetry still reaches the serial log.
    ESP_LOGE(TAG, "display unavailable; continuing without a UI");
  }

  if (!g_encoder.begin()) {
    ESP_LOGE(TAG, "encoder unavailable; input disabled");
  }
  g_encoder.setListener(onEncoderEvent);

  g_client.setTelemetryListener(onTelemetry);
  g_client.setStateListener(onConnectionState);
  if (!nvsOk) {
    ESP_LOGE(TAG, "skipping BLE start because NVS is unavailable");
  } else if (!g_client.begin()) {
    ESP_LOGE(TAG, "BLE stack failed to start");
  }

#if __has_include("freertos/FreeRTOS.h")
  const BaseType_t created = xTaskCreate(uiTask, "ui", kUiTaskStackBytes, nullptr,
                                         kUiTaskPriority, nullptr);
  if (created != pdPASS) {
    ESP_LOGE(TAG, "could not create the UI task; running it on main instead");
    uiTask(nullptr);
  }
#else
  uiTask(nullptr);
#endif
}
