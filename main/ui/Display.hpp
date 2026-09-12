// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "../ble/RaceBoxClient.hpp"
#include "../ble/RaceboxData.hpp"
#include "../common/CriticalSection.hpp"
#include "../config/Settings.hpp"
#include "LapTimer.hpp"
#include "Menu.hpp"

#include <cstdint>

#if __has_include("esp_lcd_panel_ops.h") && __has_include("lvgl.h")
  #define RACEBOX_HAVE_DISPLAY 1
  #include "driver/spi_master.h"
  #include "esp_lcd_panel_io.h"
  #include "esp_lcd_panel_ops.h"
  #include "esp_lcd_panel_vendor.h"
  #include "esp_timer.h"
  #include "lvgl.h"
#endif

namespace ktsu { namespace racebox { namespace ui {

// ILI9488 SPI panel driven through LVGL.
//
// Threading: updateTelemetry() and updateConnectionState() are called from the
// NimBLE host task and only stash data under a spinlock. Every LVGL call
// happens in loop(), on the task that owns the UI. Nothing here may be called
// into LVGL from the BLE side -- LVGL is not thread safe.
class Display {
 public:
  // Dependencies are injected so the UI can act on them from menu callbacks.
  void setSettings(ktsu::racebox::config::Settings* settings) { settings_ = settings; }
  void setLapTimer(LapTimer* timer) { lapTimer_ = timer; }

  // Returns false if the panel could not be brought up. The caller should carry
  // on headless rather than abort -- telemetry still reaches the serial log.
  bool begin();

  // Pumps LVGL and repaints when data has changed. Call from the UI task only.
  void loop();

  // --- Input events (UI task) ---
  void onRotate(int delta);
  void onClick();
  void onLongPress();

  // --- Data ingestion (BLE host task; LVGL-free) ---
  void updateTelemetry(const ktsu::racebox::ble::RaceboxData& data);
  // Link state plus the peer's identity, so the UI can name what it is talking
  // to instead of showing placeholders.
  void updateLink(ktsu::racebox::ble::ConnectionState state, const char* peerName, int8_t rssi);
  // Also called on the BLE host task: the GNSS receiver configuration the
  // device reported, shown on the About screen.
  void updateGnssConfig(const ktsu::racebox::ble::GnssConfig& config);

  bool ready() const { return inited_; }

 private:
  enum class Screen { Telemetry, Menu, About };

  void buildMenus();
  void applyBrightness();

  // --- Menu actions ---
  void actionToggleSession();
  void actionLap();
  void actionResetSession();
  void actionSetUnits(ktsu::racebox::config::SpeedUnits units);
  void actionSetBrightness(uint8_t percent);
  void actionShowAbout();

  ktsu::racebox::config::Settings* settings_ = nullptr;
  LapTimer* lapTimer_ = nullptr;

  MenuNavigator menu_{};
  Screen screen_ = Screen::Telemetry;
  bool inited_ = false;

  // Shared with the BLE task; guarded by dataLock_.
  ktsu::racebox::common::SpinLock dataLock_;
  ktsu::racebox::ble::RaceboxData pendingData_{};
  ktsu::racebox::ble::ConnectionState pendingState_ =
      ktsu::racebox::ble::ConnectionState::Idle;
  ktsu::racebox::ble::GnssConfig pendingGnssConfig_{};
  char pendingPeerName_[32] = {0};
  int8_t pendingRssi_ = 0;
  bool dataDirty_ = false;
  bool stateDirty_ = false;
  bool gnssConfigDirty_ = false;

  // UI-task-owned copies.
  ktsu::racebox::ble::RaceboxData data_{};
  ktsu::racebox::ble::GnssConfig gnssConfig_{};
  bool hasGnssConfig_ = false;
  ktsu::racebox::ble::ConnectionState connState_ =
      ktsu::racebox::ble::ConnectionState::Idle;
  char peerName_[32] = {0};
  int8_t peerRssi_ = 0;
  bool hasTelemetry_ = false;
  bool needsRepaint_ = true;

  // Repaint cadence for the running lap clock.
  static constexpr uint32_t kClockRenderIntervalMs = 100;
  uint32_t lastClockRenderMs_ = 0;

  // Menu storage. Labels for dynamic items point at the buffers below.
  MenuItem rootItems_[5]{};
  MenuItem settingsItems_[3]{};
  MenuItem unitsItems_[2]{};
  MenuItem brightnessItems_[4]{};
  Menu rootMenu_{};
  Menu settingsMenu_{};
  Menu unitsMenu_{};
  Menu brightnessMenu_{};
  char sessionItemLabel_[24] = "Start session";

#ifdef RACEBOX_HAVE_DISPLAY
  bool initPanel();
  bool initLvgl();
  void buildUi();
  void renderTelemetry();
  void renderMenu();
  void renderAbout();
  void renderStatusBar();
  static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap);
  static bool onFlushDone(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t* edata,
                          void* userCtx);

  spi_host_device_t spiHost_ = SPI2_HOST;
  esp_lcd_panel_io_handle_t ioHandle_ = nullptr;
  esp_lcd_panel_handle_t panelHandle_ = nullptr;
  esp_timer_handle_t tickTimer_ = nullptr;

  lv_display_t* disp_ = nullptr;
  uint8_t* drawBuf1_ = nullptr;
  uint8_t* drawBuf2_ = nullptr;

  // Status bar
  lv_obj_t* statusBar_ = nullptr;
  lv_obj_t* statusLabel_ = nullptr;
  lv_obj_t* statusRight_ = nullptr;
  // Telemetry view
  lv_obj_t* telemetryView_ = nullptr;
  lv_obj_t* speedLabel_ = nullptr;
  lv_obj_t* unitLabel_ = nullptr;
  lv_obj_t* detailLabel_ = nullptr;
  lv_obj_t* timerLabel_ = nullptr;
  // Menu view
  lv_obj_t* menuView_ = nullptr;
  lv_obj_t* menuTitle_ = nullptr;
  lv_obj_t* menuRows_[6] = {nullptr};
  // About view
  lv_obj_t* aboutView_ = nullptr;
  lv_obj_t* aboutLabel_ = nullptr;
#endif
};

} } } // namespaces
