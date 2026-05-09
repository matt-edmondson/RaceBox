// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "../ble/RaceBoxClient.hpp"
#include "Menu.hpp"
// Pins are referenced from implementation; keep include local there to avoid unused include

#if __has_include("esp_lcd_panel_ops.h")
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_io.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#endif

#if __has_include("lvgl.h")
#include "lvgl.h"
#endif

namespace ktsu { namespace racebox { namespace ui {

class Display {
 public:
  void begin();
  void loop();
  void onRotate(int delta);
  void onClick();
  void onLongPress();
  void updateTelemetry(const ktsu::racebox::ble::RaceboxData& data);

 private:
  ktsu::racebox::ui::MenuNavigator menu_{};
  ktsu::racebox::ble::RaceboxData lastData_{};
  void draw();
  void drawMenu();
  void buildMenus();

    bool inited_ = false;

#if __has_include("esp_lcd_panel_ops.h") && __has_include("lvgl.h")
    // SPI + Panel
    spi_host_device_t spiHost_ = SPI2_HOST;
    esp_lcd_panel_io_handle_t ioHandle_ = nullptr;
    esp_lcd_panel_handle_t panelHandle_ = nullptr;
    // LVGL
    lv_disp_t* disp_ = nullptr;
    lv_obj_t* labelSpeed_ = nullptr;
    lv_obj_t* labelAlt_ = nullptr;
    lv_color_t* buf1_ = nullptr;
    lv_color_t* buf2_ = nullptr;
    esp_timer_handle_t lvglTickTimer_ = nullptr;

    static void lvglFlushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map);
    void initPanelAndLvgl();
#endif
};

} } } // namespaces


