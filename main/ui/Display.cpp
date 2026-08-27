// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "Display.hpp"

#include "../common/IdfCompat.hpp"
#include "../config/Pins.hpp"

#include <cstdio>
#include <cstring>

#ifdef RACEBOX_HAVE_DISPLAY
  #include "driver/ledc.h"
  #include "esp_heap_caps.h"
#endif

namespace ktsu { namespace racebox { namespace ui {

using ktsu::racebox::ble::ConnectionState;
using ktsu::racebox::config::SpeedUnits;

namespace {
constexpr const char* TAG = "Display";

// Format milliseconds as m:ss.t, which is what a lap readout wants.
void formatDuration(uint32_t ms, char* out, size_t outLen) {
  const uint32_t tenths = (ms / 100) % 10;
  const uint32_t totalSeconds = ms / 1000;
  const uint32_t minutes = totalSeconds / 60;
  const uint32_t seconds = totalSeconds % 60;
  snprintf(out, outLen, "%lu:%02lu.%lu", static_cast<unsigned long>(minutes),
           static_cast<unsigned long>(seconds), static_cast<unsigned long>(tenths));
}
} // namespace

// ---------------------------------------------------------------------------
// Data ingestion (BLE host task). Deliberately free of LVGL calls -- see #28.
// ---------------------------------------------------------------------------

void Display::updateTelemetry(const ktsu::racebox::ble::RaceboxData& data) {
  ktsu::racebox::common::LockGuard guard(dataLock_);
  pendingData_ = data;
  dataDirty_ = true;
}

void Display::updateLink(ConnectionState state, const char* peerName, int8_t rssi) {
  ktsu::racebox::common::LockGuard guard(dataLock_);
  pendingState_ = state;
  pendingRssi_ = rssi;
  if (peerName) {
    strncpy(pendingPeerName_, peerName, sizeof(pendingPeerName_) - 1);
    pendingPeerName_[sizeof(pendingPeerName_) - 1] = '\0';
  }
  stateDirty_ = true;
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------

void Display::buildMenus() {
  unitsItems_[0].label = "km/h";
  unitsItems_[0].action = [this]() { actionSetUnits(SpeedUnits::Kmh); };
  unitsItems_[1].label = "mph";
  unitsItems_[1].action = [this]() { actionSetUnits(SpeedUnits::Mph); };
  unitsMenu_.title = "Speed units";
  unitsMenu_.items = unitsItems_;
  unitsMenu_.itemCount = 2;

  brightnessItems_[0].label = "25%";
  brightnessItems_[0].action = [this]() { actionSetBrightness(25); };
  brightnessItems_[1].label = "50%";
  brightnessItems_[1].action = [this]() { actionSetBrightness(50); };
  brightnessItems_[2].label = "75%";
  brightnessItems_[2].action = [this]() { actionSetBrightness(75); };
  brightnessItems_[3].label = "100%";
  brightnessItems_[3].action = [this]() { actionSetBrightness(100); };
  brightnessMenu_.title = "Brightness";
  brightnessMenu_.items = brightnessItems_;
  brightnessMenu_.itemCount = 4;

  settingsItems_[0].label = "Brightness";
  settingsItems_[0].submenu = &brightnessMenu_;
  settingsItems_[1].label = "Speed units";
  settingsItems_[1].submenu = &unitsMenu_;
  settingsItems_[2].label = "About";
  settingsItems_[2].action = [this]() { actionShowAbout(); };
  settingsMenu_.title = "Settings";
  settingsMenu_.items = settingsItems_;
  settingsMenu_.itemCount = 3;

  // The session item's label tracks the timer state, so it points at a buffer
  // rather than a literal.
  rootItems_[0].label = sessionItemLabel_;
  rootItems_[0].action = [this]() { actionToggleSession(); };
  rootItems_[1].label = "Lap";
  rootItems_[1].action = [this]() { actionLap(); };
  rootItems_[2].label = "Reset session";
  rootItems_[2].action = [this]() { actionResetSession(); };
  rootItems_[3].label = "Settings";
  rootItems_[3].submenu = &settingsMenu_;
  rootItems_[4].label = "Close menu";
  rootItems_[4].action = [this]() { screen_ = Screen::Telemetry; };
  rootMenu_.title = "Menu";
  rootMenu_.items = rootItems_;
  rootMenu_.itemCount = 5;

  menu_.setRoot(&rootMenu_);
}

void Display::actionToggleSession() {
  if (!lapTimer_) return;
  if (lapTimer_->running()) {
    lapTimer_->stop();
  } else {
    lapTimer_->start();
  }
  // Leave the menu so the user immediately sees the running timer.
  screen_ = Screen::Telemetry;
}

void Display::actionLap() {
  if (!lapTimer_ || !lapTimer_->running()) return;
  lapTimer_->lap();
  screen_ = Screen::Telemetry;
}

void Display::actionResetSession() {
  if (!lapTimer_) return;
  lapTimer_->reset();
}

void Display::actionSetUnits(SpeedUnits units) {
  if (!settings_) return;
  settings_->setSpeedUnits(units);
  settings_->save();
  menu_.back();
}

void Display::actionSetBrightness(uint8_t percent) {
  if (!settings_) return;
  settings_->setBrightness(percent);
  settings_->save();
  applyBrightness();
  menu_.back();
}

void Display::actionShowAbout() { screen_ = Screen::About; }

// ---------------------------------------------------------------------------
// Input (UI task)
// ---------------------------------------------------------------------------

void Display::onRotate(int delta) {
  if (screen_ == Screen::Menu) menu_.rotate(delta);
  needsRepaint_ = true;
}

void Display::onClick() {
  switch (screen_) {
    case Screen::Telemetry:
      screen_ = Screen::Menu;
      break;
    case Screen::Menu:
      menu_.confirm();
      break;
    case Screen::About:
      screen_ = Screen::Menu;
      break;
  }
  needsRepaint_ = true;
}

void Display::onLongPress() {
  switch (screen_) {
    case Screen::Telemetry:
      // A hold on the main screen is a shortcut for lap when timing.
      actionLap();
      break;
    case Screen::Menu:
      // Step out of a submenu, or close the menu entirely at the root.
      if (menu_.canGoBack()) {
        menu_.back();
      } else {
        screen_ = Screen::Telemetry;
      }
      break;
    case Screen::About:
      screen_ = Screen::Menu;
      break;
  }
  needsRepaint_ = true;
}

// ---------------------------------------------------------------------------
// Frame loop
// ---------------------------------------------------------------------------

void Display::loop() {
  // Pull anything the BLE task left for us, outside of any LVGL work.
  bool freshSample = false;
  {
    ktsu::racebox::common::LockGuard guard(dataLock_);
    if (dataDirty_) {
      data_ = pendingData_;
      dataDirty_ = false;
      hasTelemetry_ = true;
      needsRepaint_ = true;
      freshSample = true;
    }
    if (stateDirty_) {
      connState_ = pendingState_;
      stateDirty_ = false;
      peerRssi_ = pendingRssi_;
      memcpy(peerName_, pendingPeerName_, sizeof(peerName_));
      needsRepaint_ = true;
    }
  }

  // Only advance timing on a genuinely new sample; re-feeding the same frame
  // every 5 ms would be pure waste.
  if (lapTimer_ && freshSample) lapTimer_->update(data_);

#ifdef RACEBOX_HAVE_DISPLAY
  if (!inited_) return;

  if (needsRepaint_) {
    needsRepaint_ = false;
    renderStatusBar();
    switch (screen_) {
      case Screen::Telemetry: renderTelemetry(); break;
      case Screen::Menu: renderMenu(); break;
      case Screen::About: renderAbout(); break;
    }
  } else if (lapTimer_ && lapTimer_->running() && screen_ == Screen::Telemetry) {
    // Keep the clock ticking between telemetry frames, but at a readable rate
    // rather than once per 5 ms loop.
    const uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    if (now - lastClockRenderMs_ >= kClockRenderIntervalMs) {
      lastClockRenderMs_ = now;
      renderTelemetry();
    }
  }

  lv_timer_handler();
#endif
}

#ifndef RACEBOX_HAVE_DISPLAY

// Host / lint build: no panel, no LVGL.
bool Display::begin() {
  buildMenus();
  ESP_LOGW(TAG, "built without LVGL/esp_lcd; display disabled");
  return false;
}

void Display::applyBrightness() {}

#else

// ---------------------------------------------------------------------------
// Panel + LVGL bring-up
// ---------------------------------------------------------------------------

bool Display::begin() {
  buildMenus();
  if (!initPanel()) return false;
  if (!initLvgl()) return false;
  buildUi();
  applyBrightness();
  inited_ = true;
  needsRepaint_ = true;
  ESP_LOGI(TAG, "display ready (%dx%d)", ktsu::Panel::width, ktsu::Panel::height);
  return true;
}

bool Display::initPanel() {
  spi_bus_config_t busCfg = {};
  busCfg.mosi_io_num = ktsu::Pins::tftMosi;
  busCfg.miso_io_num = ktsu::Pins::tftMiso;
  busCfg.sclk_io_num = ktsu::Pins::tftSclk;
  busCfg.quadwp_io_num = -1;
  busCfg.quadhd_io_num = -1;
  // Worst-case single flush: full width, buffer height, 3 bytes per pixel.
  busCfg.max_transfer_sz = ktsu::Panel::width * ktsu::Panel::bufferLines * 3;

  esp_err_t err = spi_bus_initialize(spiHost_, &busCfg, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(err));
    return false;
  }

  esp_lcd_panel_io_spi_config_t ioCfg = {};
  ioCfg.cs_gpio_num = ktsu::Pins::tftCs;
  ioCfg.dc_gpio_num = ktsu::Pins::tftDc;
  ioCfg.spi_mode = 0;
  ioCfg.pclk_hz = ktsu::Panel::pixelClockHz;
  ioCfg.trans_queue_depth = 10;
  ioCfg.lcd_cmd_bits = 8;
  ioCfg.lcd_param_bits = 8;
  ioCfg.on_color_trans_done = &Display::onFlushDone;
  ioCfg.user_ctx = this;

  err = esp_lcd_new_panel_io_spi(reinterpret_cast<esp_lcd_spi_bus_handle_t>(spiHost_), &ioCfg,
                                 &ioHandle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi failed: %s", esp_err_to_name(err));
    return false;
  }

  esp_lcd_panel_dev_config_t panelCfg = {};
  panelCfg.reset_gpio_num = ktsu::Pins::tftRst;
  panelCfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  // The ILI9488 cannot accept 16-bit pixels over SPI -- it requires 18-bit
  // (RGB666). The driver converts LVGL's RGB565 output for us.
  panelCfg.bits_per_pixel = 18;

  // The ILI9488 driver converts LVGL's RGB565 output into the RGB666 the panel
  // demands over SPI, and needs a buffer big enough for one full flush at three
  // bytes per pixel.
  const size_t conversionBufBytes =
      static_cast<size_t>(ktsu::Panel::width) * ktsu::Panel::bufferLines * 3;
  err = esp_lcd_new_panel_ili9488(ioHandle_, &panelCfg, conversionBufBytes, &panelHandle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_lcd_new_panel_ili9488 failed: %s", esp_err_to_name(err));
    return false;
  }

  if ((err = esp_lcd_panel_reset(panelHandle_)) != ESP_OK ||
      (err = esp_lcd_panel_init(panelHandle_)) != ESP_OK ||
      (err = esp_lcd_panel_disp_on_off(panelHandle_, true)) != ESP_OK) {
    ESP_LOGE(TAG, "panel bring-up failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

bool Display::initLvgl() {
  lv_init();

  const size_t pixels = static_cast<size_t>(ktsu::Panel::width) * ktsu::Panel::bufferLines;
  // LVGL renders RGB565 here, so two bytes per pixel -- NOT sizeof(lv_color_t),
  // which is three bytes in LVGL 9 and would mis-size the buffer.
  const size_t bufBytes = pixels * 2;

  // Internal DMA-capable RAM: these buffers are handed straight to the SPI
  // driver, and the board is not required to have PSRAM.
  drawBuf1_ = static_cast<uint8_t*>(heap_caps_malloc(bufBytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
  drawBuf2_ = static_cast<uint8_t*>(heap_caps_malloc(bufBytes, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));
  if (!drawBuf1_ || !drawBuf2_) {
    ESP_LOGE(TAG, "could not allocate two %u-byte LVGL draw buffers",
             static_cast<unsigned>(bufBytes));
    heap_caps_free(drawBuf1_);
    heap_caps_free(drawBuf2_);
    drawBuf1_ = drawBuf2_ = nullptr;
    return false;
  }

  disp_ = lv_display_create(ktsu::Panel::width, ktsu::Panel::height);
  if (!disp_) {
    ESP_LOGE(TAG, "lv_display_create failed");
    return false;
  }
  lv_display_set_color_format(disp_, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp_, drawBuf1_, drawBuf2_, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp_, &Display::flushCb);
  lv_display_set_user_data(disp_, this);

  esp_timer_create_args_t tickArgs{};
  tickArgs.callback = [](void*) { lv_tick_inc(1); };
  tickArgs.dispatch_method = ESP_TIMER_TASK;
  tickArgs.name = "lvgl_tick";
  esp_err_t err = esp_timer_create(&tickArgs, &tickTimer_);
  if (err == ESP_OK) err = esp_timer_start_periodic(tickTimer_, 1000);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "LVGL tick timer failed: %s", esp_err_to_name(err));
    return false;
  }
  return true;
}

void Display::flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
  auto* self = static_cast<Display*>(lv_display_get_user_data(disp));
  if (!self || !self->panelHandle_ || !area) {
    lv_display_flush_ready(disp);
    return;
  }
  // Asynchronous: completion is signalled from onFlushDone(), NOT here. Calling
  // lv_display_flush_ready() now would hand the buffer back to LVGL while DMA
  // was still reading out of it.
  const esp_err_t err = esp_lcd_panel_draw_bitmap(self->panelHandle_, area->x1, area->y1,
                                                  area->x2 + 1, area->y2 + 1, pxMap);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "draw_bitmap failed: %s", esp_err_to_name(err));
    lv_display_flush_ready(disp); // don't wedge LVGL waiting for a callback
  }
}

bool Display::onFlushDone(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t* edata,
                          void* userCtx) {
  (void)io;
  (void)edata;
  auto* self = static_cast<Display*>(userCtx);
  if (self && self->disp_) lv_display_flush_ready(self->disp_);
  return false;
}

void Display::applyBrightness() {
  if (ktsu::Pins::tftBl < 0) return;
  const uint8_t percent = settings_ ? settings_->brightness() : 100;

  static bool ledcReady = false;
  if (!ledcReady) {
    ledc_timer_config_t timerCfg{};
    timerCfg.speed_mode = LEDC_LOW_SPEED_MODE;
    timerCfg.duty_resolution = LEDC_TIMER_10_BIT;
    timerCfg.timer_num = LEDC_TIMER_0;
    timerCfg.freq_hz = 5000;
    timerCfg.clk_cfg = LEDC_AUTO_CLK;
    esp_err_t err = ledc_timer_config(&timerCfg);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "ledc_timer_config failed (%s); falling back to on/off",
               esp_err_to_name(err));
      gpio_config_t io{};
      io.intr_type = GPIO_INTR_DISABLE;
      io.mode = GPIO_MODE_OUTPUT;
      io.pull_up_en = GPIO_PULLUP_DISABLE;
      io.pull_down_en = GPIO_PULLDOWN_DISABLE;
      io.pin_bit_mask = (1ULL << ktsu::Pins::tftBl);
      if ((err = gpio_config(&io)) != ESP_OK) {
        ESP_LOGE(TAG, "backlight gpio_config failed: %s", esp_err_to_name(err));
        return;
      }
      if ((err = gpio_set_level(static_cast<gpio_num_t>(ktsu::Pins::tftBl), 1)) != ESP_OK) {
        ESP_LOGE(TAG, "backlight gpio_set_level failed: %s", esp_err_to_name(err));
      }
      return;
    }

    ledc_channel_config_t chanCfg{};
    chanCfg.gpio_num = ktsu::Pins::tftBl;
    chanCfg.speed_mode = LEDC_LOW_SPEED_MODE;
    chanCfg.channel = LEDC_CHANNEL_0;
    chanCfg.timer_sel = LEDC_TIMER_0;
    chanCfg.duty = 0;
    chanCfg.hpoint = 0;
    err = ledc_channel_config(&chanCfg);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "ledc_channel_config failed: %s", esp_err_to_name(err));
      return;
    }
    ledcReady = true;
  }

  const uint32_t duty = (1023u * percent) / 100u;
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

// ---------------------------------------------------------------------------
// Widgets
// ---------------------------------------------------------------------------

void Display::buildUi() {
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x101014), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

  // --- Status bar ---
  statusBar_ = lv_obj_create(scr);
  lv_obj_remove_style_all(statusBar_);
  lv_obj_set_size(statusBar_, ktsu::Panel::width, 28);
  lv_obj_align(statusBar_, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_color(statusBar_, lv_color_hex(0x1E1E28), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(statusBar_, LV_OPA_COVER, LV_PART_MAIN);

  statusLabel_ = lv_label_create(statusBar_);
  lv_obj_align(statusLabel_, LV_ALIGN_LEFT_MID, 8, 0);
  lv_obj_set_style_text_color(statusLabel_, lv_color_hex(0xE0E0E8), LV_PART_MAIN);
  lv_label_set_text(statusLabel_, "Starting...");

  statusRight_ = lv_label_create(statusBar_);
  lv_obj_align(statusRight_, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_set_style_text_color(statusRight_, lv_color_hex(0xA0A0B0), LV_PART_MAIN);
  lv_label_set_text(statusRight_, "");

  // --- Telemetry view ---
  telemetryView_ = lv_obj_create(scr);
  lv_obj_remove_style_all(telemetryView_);
  lv_obj_set_size(telemetryView_, ktsu::Panel::width, ktsu::Panel::height - 28);
  lv_obj_align(telemetryView_, LV_ALIGN_TOP_MID, 0, 28);

  speedLabel_ = lv_label_create(telemetryView_);
  lv_obj_align(speedLabel_, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_set_style_text_color(speedLabel_, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(speedLabel_, &lv_font_montserrat_48, LV_PART_MAIN);
  lv_label_set_text(speedLabel_, "--");

  unitLabel_ = lv_label_create(telemetryView_);
  lv_obj_align(unitLabel_, LV_ALIGN_TOP_MID, 0, 78);
  lv_obj_set_style_text_color(unitLabel_, lv_color_hex(0x8080A0), LV_PART_MAIN);
  lv_label_set_text(unitLabel_, "km/h");

  timerLabel_ = lv_label_create(telemetryView_);
  lv_obj_align(timerLabel_, LV_ALIGN_TOP_MID, 0, 110);
  lv_obj_set_style_text_color(timerLabel_, lv_color_hex(0x60D0FF), LV_PART_MAIN);
  lv_obj_set_style_text_font(timerLabel_, &lv_font_montserrat_28, LV_PART_MAIN);
  lv_label_set_text(timerLabel_, "");

  detailLabel_ = lv_label_create(telemetryView_);
  lv_obj_align(detailLabel_, LV_ALIGN_BOTTOM_LEFT, 10, -10);
  lv_obj_set_style_text_color(detailLabel_, lv_color_hex(0xC0C0D0), LV_PART_MAIN);
  lv_label_set_text(detailLabel_, "");

  // --- Menu view ---
  menuView_ = lv_obj_create(scr);
  lv_obj_remove_style_all(menuView_);
  lv_obj_set_size(menuView_, ktsu::Panel::width, ktsu::Panel::height - 28);
  lv_obj_align(menuView_, LV_ALIGN_TOP_MID, 0, 28);
  lv_obj_add_flag(menuView_, LV_OBJ_FLAG_HIDDEN);

  menuTitle_ = lv_label_create(menuView_);
  lv_obj_align(menuTitle_, LV_ALIGN_TOP_LEFT, 12, 6);
  lv_obj_set_style_text_color(menuTitle_, lv_color_hex(0x60D0FF), LV_PART_MAIN);
  lv_label_set_text(menuTitle_, "Menu");

  for (size_t i = 0; i < (sizeof(menuRows_) / sizeof(menuRows_[0])); ++i) {
    lv_obj_t* row = lv_label_create(menuView_);
    lv_obj_set_width(row, ktsu::Panel::width - 24);
    lv_obj_align(row, LV_ALIGN_TOP_LEFT, 12, 40 + static_cast<int>(i) * 32);
    lv_obj_set_style_text_color(row, lv_color_hex(0xD0D0DC), LV_PART_MAIN);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x2A2A3A), LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_text(row, "");
    menuRows_[i] = row;
  }

  // --- About view ---
  aboutView_ = lv_obj_create(scr);
  lv_obj_remove_style_all(aboutView_);
  lv_obj_set_size(aboutView_, ktsu::Panel::width, ktsu::Panel::height - 28);
  lv_obj_align(aboutView_, LV_ALIGN_TOP_MID, 0, 28);
  lv_obj_add_flag(aboutView_, LV_OBJ_FLAG_HIDDEN);

  aboutLabel_ = lv_label_create(aboutView_);
  lv_obj_align(aboutLabel_, LV_ALIGN_TOP_LEFT, 12, 10);
  lv_obj_set_style_text_color(aboutLabel_, lv_color_hex(0xC0C0D0), LV_PART_MAIN);
  lv_label_set_text(aboutLabel_, "");
}

void Display::renderStatusBar() {
  if (!statusLabel_) return;

  const char* stateText = ktsu::racebox::ble::toString(connState_);
  lv_label_set_text(statusLabel_, stateText);

  lv_color_t colour = lv_color_hex(0xE0A000); // amber: working on it
  if (connState_ == ConnectionState::Streaming) colour = lv_color_hex(0x40D060);
  if (connState_ == ConnectionState::Disconnected || connState_ == ConnectionState::Idle) {
    colour = lv_color_hex(0xE05050);
  }
  lv_obj_set_style_text_color(statusLabel_, colour, LV_PART_MAIN);

  char right[64];
  if (hasTelemetry_) {
    snprintf(right, sizeof(right), "%u sat  %u%%%s  %d dBm",
             static_cast<unsigned>(data_.satellites),
             static_cast<unsigned>(data_.batteryPercent), data_.charging ? "+" : "",
             static_cast<int>(peerRssi_));
  } else {
    snprintf(right, sizeof(right), "no data");
  }
  lv_label_set_text(statusRight_, right);
}

void Display::renderTelemetry() {
  lv_obj_add_flag(menuView_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(aboutView_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(telemetryView_, LV_OBJ_FLAG_HIDDEN);

  char buf[96];
  if (hasTelemetry_) {
    const float shown = settings_ ? settings_->displaySpeed(data_.speedKmh) : data_.speedKmh;
    snprintf(buf, sizeof(buf), "%.1f", static_cast<double>(shown));
  } else {
    snprintf(buf, sizeof(buf), "--");
  }
  lv_label_set_text(speedLabel_, buf);
  lv_label_set_text(unitLabel_, settings_ ? settings_->speedUnitLabel() : "km/h");

  if (lapTimer_ && (lapTimer_->running() || lapTimer_->lapCount() > 0)) {
    char lap[16];
    char best[16];
    formatDuration(lapTimer_->currentLapMs(), lap, sizeof(lap));
    formatDuration(lapTimer_->bestLapMs(), best, sizeof(best));
    if (lapTimer_->lapCount() > 0) {
      snprintf(buf, sizeof(buf), "L%u  %s   best %s",
               static_cast<unsigned>(lapTimer_->lapCount() + 1), lap, best);
    } else {
      snprintf(buf, sizeof(buf), "L1  %s", lap);
    }
    lv_label_set_text(timerLabel_, buf);
  } else {
    lv_label_set_text(timerLabel_, "");
  }

  if (hasTelemetry_) {
    snprintf(buf, sizeof(buf), "Alt %.0f m   Hdg %.0f\xC2\xB0   HDOP %.1f\nLat %.5f  Lon %.5f",
             static_cast<double>(data_.mslAltitudeM), static_cast<double>(data_.headingDeg),
             static_cast<double>(data_.pdop), data_.latitudeDeg, data_.longitudeDeg);
  } else {
    snprintf(buf, sizeof(buf), "Waiting for a RaceBox...");
  }
  lv_label_set_text(detailLabel_, buf);
}

void Display::renderMenu() {
  lv_obj_add_flag(telemetryView_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(aboutView_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(menuView_, LV_OBJ_FLAG_HIDDEN);

  // Keep the dynamic session label in step with the timer.
  if (lapTimer_) {
    snprintf(sessionItemLabel_, sizeof(sessionItemLabel_), "%s session",
             lapTimer_->running() ? "Stop" : "Start");
  }

  const Menu* current = menu_.currentMenu();
  lv_label_set_text(menuTitle_, current && current->title ? current->title : "Menu");

  const int rowCount = static_cast<int>(sizeof(menuRows_) / sizeof(menuRows_[0]));
  const int itemCount = current ? current->itemCount : 0;
  const int selected = menu_.currentIndex();

  // Scroll so the cursor stays visible on menus longer than the row budget.
  int first = 0;
  if (itemCount > rowCount) {
    first = selected - rowCount / 2;
    if (first < 0) first = 0;
    if (first > itemCount - rowCount) first = itemCount - rowCount;
  }

  for (int row = 0; row < rowCount; ++row) {
    const int itemIndex = first + row;
    lv_obj_t* obj = menuRows_[row];
    if (!current || itemIndex >= itemCount) {
      lv_label_set_text(obj, "");
      lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
      continue;
    }

    const MenuItem& item = current->items[itemIndex];
    char line[64];
    snprintf(line, sizeof(line), "%s%s", item.label ? item.label : "",
             item.submenu ? "  >" : "");
    lv_label_set_text(obj, line);

    const bool isSelected = (itemIndex == selected);
    lv_obj_set_style_bg_opa(obj, isSelected ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, lv_color_hex(isSelected ? 0xFFFFFF : 0xB0B0C0),
                                LV_PART_MAIN);
  }
}

void Display::renderAbout() {
  lv_obj_add_flag(telemetryView_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(menuView_, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(aboutView_, LV_OBJ_FLAG_HIDDEN);

  char buf[256];
  snprintf(buf, sizeof(buf),
           "RaceBox Mini Interface\n\n"
           "Device: %s\nRSSI: %d dBm\nLink: %s\n\n"
           "Units: %s\nBrightness: %u%%\n\n"
           "Click or hold to go back",
           peerName_[0] ? peerName_ : "-", static_cast<int>(peerRssi_),
           ktsu::racebox::ble::toString(connState_),
           settings_ ? settings_->speedUnitLabel() : "km/h",
           static_cast<unsigned>(settings_ ? settings_->brightness() : 100));
  lv_label_set_text(aboutLabel_, buf);
}

#endif // RACEBOX_HAVE_DISPLAY

} } } // namespaces
