// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "Display.hpp"
#include "Menu.hpp"

#include "../common/IdfCompat.hpp"
// Pins included in Display.hpp implementation section

using namespace ktsu::racebox::ui;

static const char* TAG = "Display";

void Display::begin() {
  ESP_LOGI(TAG, "Display init");
#if __has_include("esp_lcd_panel_ops.h") && __has_include("lvgl.h")
  initPanelAndLvgl();
#endif
  buildMenus();
  draw();
}

void Display::loop() {
#if __has_include("lvgl.h")
  if (inited_) { lv_timer_handler(); }
#endif
}

void Display::onRotate(int delta) { menu_.rotate(delta); draw(); }

void Display::onClick() { menu_.confirm(); draw(); }

void Display::onLongPress() { menu_.back(); draw(); }

void Display::updateTelemetry(const ktsu::racebox::ble::RaceboxData& data) { lastData_ = data; draw(); }

void Display::draw() {
#if __has_include("lvgl.h")
  if (!inited_) return;
  if (!labelSpeed_) { labelSpeed_ = lv_label_create(lv_scr_act()); lv_obj_align(labelSpeed_, LV_ALIGN_TOP_MID, 0, 10); }
  if (!labelAlt_) { labelAlt_ = lv_label_create(lv_scr_act()); lv_obj_align(labelAlt_, LV_ALIGN_TOP_MID, 0, 60); }
  char buf[64];
  snprintf(buf, sizeof(buf), "Speed: %.1f km/h  Sats:%lu", static_cast<double>(lastData_.speedKmh), static_cast<unsigned long>(lastData_.sats));
  lv_label_set_text(labelSpeed_, buf);
  snprintf(buf, sizeof(buf), "Alt: %.1f m", static_cast<double>(lastData_.altitudeM));
  lv_label_set_text(labelAlt_, buf);
#else
  // Stub
  (void)lastData_;
#endif
}

void Display::drawMenu() {
  (void)menu_;
}

namespace {
  using namespace ktsu::racebox::ui;
  static Menu rootMenu;
  static Menu settingsMenu;
  static Menu unitsMenu;
  static MenuItem rootItems[3];
  static MenuItem settingsItems[3];
  static MenuItem unitsItems[2];

  void initMenusOnce() {
    unitsItems[0].label = "km/h";
    unitsItems[1].label = "mph";
    unitsMenu.title = "Speed Units";
    unitsMenu.items = unitsItems; unitsMenu.itemCount = 2;

    settingsItems[0].label = "Brightness";
    settingsItems[1].label = "Units"; settingsItems[1].submenu = &unitsMenu;
    settingsItems[2].label = "About";
    settingsMenu.title = "Settings";
    settingsMenu.items = settingsItems; settingsMenu.itemCount = 3;

    rootItems[0].label = "Start";
    rootItems[1].label = "Lap";
    rootItems[2].label = "Settings"; rootItems[2].submenu = &settingsMenu;
    rootMenu.title = "Main Menu";
    rootMenu.items = rootItems; rootMenu.itemCount = 3;
  }
}

void Display::buildMenus() {
  static bool initialized = false;
  if (!initialized) { initMenusOnce(); initialized = true; }
  menu_.setRoot(&rootMenu);
}

#if __has_include("esp_lcd_panel_ops.h") && __has_include("lvgl.h")
void Display::initPanelAndLvgl() {
  using namespace ktsu::Pins;
  // SPI bus
  spi_bus_config_t buscfg = {};
  buscfg.mosi_io_num = tftMosi; buscfg.miso_io_num = tftMiso; buscfg.sclk_io_num = tftSclk; buscfg.quadwp_io_num = -1; buscfg.quadhd_io_num = -1;
  buscfg.max_transfer_sz = 480 * 40 * 2; // partial updates
  ESP_ERROR_CHECK(spi_bus_initialize(spiHost_, &buscfg, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_spi_config_t io_cfg = {};
  io_cfg.cs_gpio_num = tftCs; io_cfg.dc_gpio_num = tftDc; io_cfg.spi_mode = 0; io_cfg.pclk_hz = 40 * 1000 * 1000; io_cfg.trans_queue_depth = 10; io_cfg.lcd_cmd_bits = 8; io_cfg.lcd_param_bits = 8;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)spiHost_, &io_cfg, &ioHandle_));

  esp_lcd_panel_dev_config_t panel_cfg = {};
  panel_cfg.reset_gpio_num = tftRst; panel_cfg.rgb_endian = LCD_RGB_ENDIAN_RGB; panel_cfg.bits_per_pixel = 16;
  ESP_ERROR_CHECK(esp_lcd_new_panel_ili9488(ioHandle_, &panel_cfg, &panelHandle_));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panelHandle_));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panelHandle_));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panelHandle_, true));

  // Backlight
  if (tftBl >= 0) {
    gpio_config_t io{}; io.intr_type = GPIO_INTR_DISABLE; io.mode = GPIO_MODE_OUTPUT; io.pull_up_en = GPIO_PULLUP_DISABLE; io.pull_down_en = GPIO_PULLDOWN_DISABLE; io.pin_bit_mask = (1ULL << tftBl); gpio_config(&io); gpio_set_level((gpio_num_t)tftBl, 1);
  }

  // LVGL init
  lv_init();
  // Allocate draw buffers
  size_t buf_pixels = 480 * 20; // 20 lines
  buf1_ = static_cast<lv_color_t*>(heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  buf2_ = static_cast<lv_color_t*>(heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  lv_display_t* disp = lv_display_create(480, 320);
  lv_display_set_buffers(disp, buf1_, buf2_, buf_pixels * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, Display::lvglFlushCb);
  lv_display_set_driver_data(disp, panelHandle_);
  disp_ = disp;

  // LVGL tick timer (1ms)
  esp_timer_create_args_t targs{}; targs.callback = [](void* arg){ lv_tick_inc(1); }; targs.arg = nullptr; targs.dispatch_method = ESP_TIMER_TASK; targs.name = "lvgl_tick";
  ESP_ERROR_CHECK(esp_timer_create(&targs, &lvglTickTimer_));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvglTickTimer_, 1000));

  inited_ = true;
}

void Display::lvglFlushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  // Convert LVGL area to ILI9488 flush
  if (!area) { lv_display_flush_ready(disp); return; }
  int x1 = area->x1, y1 = area->y1, x2 = area->x2, y2 = area->y2;
  // esp_lcd uses 16-bit color data; LVGL buffer already 16-bit (RGB565)
  esp_lcd_panel_draw_bitmap((esp_lcd_panel_handle_t)lv_display_get_driver_data(disp), x1, y1, x2 + 1, y2 + 1, px_map);
  lv_display_flush_ready(disp);
}
#endif


