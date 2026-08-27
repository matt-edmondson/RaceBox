// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "Settings.hpp"

#include <algorithm>

#if __has_include("nvs.h") && __has_include("nvs_flash.h")
  #define RACEBOX_HAVE_NVS 1
  #include "nvs.h"
  #include "nvs_flash.h"
  #include "esp_log.h"
#endif

namespace ktsu { namespace racebox { namespace config {

namespace {
constexpr float kKmhPerMph = 1.609344f;
#ifdef RACEBOX_HAVE_NVS
constexpr const char* kNamespace = "racebox";
constexpr const char* kKeyUnits = "units";
constexpr const char* kKeyBrightness = "bright";
constexpr const char* TAG = "Settings";
#endif
} // namespace

void Settings::setBrightness(uint8_t percent) {
  brightness_ = std::clamp<uint8_t>(percent, kMinBrightness, kMaxBrightness);
}

void Settings::adjustBrightness(int steps) {
  const int next = static_cast<int>(brightness_) + steps * static_cast<int>(kBrightnessStep);
  brightness_ = static_cast<uint8_t>(
      std::clamp(next, static_cast<int>(kMinBrightness), static_cast<int>(kMaxBrightness)));
}

float Settings::displaySpeed(float speedKmh) const {
  return speedUnits_ == SpeedUnits::Mph ? speedKmh / kKmhPerMph : speedKmh;
}

const char* Settings::speedUnitLabel() const {
  return speedUnits_ == SpeedUnits::Mph ? "mph" : "km/h";
}

#ifdef RACEBOX_HAVE_NVS

void Settings::load() {
  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(kNamespace, NVS_READONLY, &handle);
  if (err != ESP_OK) {
    // Absent namespace on first boot is expected; keep the defaults.
    ESP_LOGI(TAG, "no stored settings (%s); using defaults", esp_err_to_name(err));
    return;
  }
  uint8_t units = static_cast<uint8_t>(speedUnits_);
  if (nvs_get_u8(handle, kKeyUnits, &units) == ESP_OK) {
    speedUnits_ = (units == static_cast<uint8_t>(SpeedUnits::Mph)) ? SpeedUnits::Mph
                                                                  : SpeedUnits::Kmh;
  }
  uint8_t brightness = brightness_;
  if (nvs_get_u8(handle, kKeyBrightness, &brightness) == ESP_OK) {
    setBrightness(brightness);
  }
  nvs_close(handle);
}

void Settings::save() const {
  nvs_handle_t handle = 0;
  esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    return;
  }
  nvs_set_u8(handle, kKeyUnits, static_cast<uint8_t>(speedUnits_));
  nvs_set_u8(handle, kKeyBrightness, brightness_);
  err = nvs_commit(handle);
  if (err != ESP_OK) ESP_LOGW(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
  nvs_close(handle);
}

#else

// Host build: settings are in-memory only.
void Settings::load() {}
void Settings::save() const {}

#endif

} } } // namespaces
