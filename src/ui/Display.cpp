// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "Display.hpp"
#include <Arduino.h>
#include "config/Pins.hpp"
#include "ui/Menu.hpp"

using namespace ktsu::racebox::ui;
using namespace ktsu::Pins;

LGFX::LGFX() {
  {
    auto cfg = _bus_instance.config();
    cfg.spi_host = SPI3_HOST; // VSPI on S3
    cfg.spi_mode = 0;
    cfg.freq_write = 40000000;
    cfg.pin_sclk = tftSclk;
    cfg.pin_mosi = tftMosi;
    cfg.pin_miso = tftMiso;
    cfg.pin_dc   = tftDc;
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);
  }

  {
    auto cfg = _panel_instance.config();
    cfg.pin_cs   = tftCs;
    cfg.pin_rst  = tftRst;
    cfg.pin_busy = -1;
    cfg.panel_width  = 320;   // ILI9488 size
    cfg.panel_height = 480;
    cfg.memory_width  = 320;
    cfg.memory_height = 480;
    cfg.offset_x = 0;
    cfg.offset_y = 0;
    cfg.offset_rotation = 0;
    cfg.dummy_read_pixel = 8;
    cfg.dummy_read_bits = 1;
    cfg.readable = false;
    cfg.invert = false;       // change to true if colors look inverted
    cfg.rgb_order = true;     // ILI9488 is typically BGR
    cfg.dlen_16bit = false;
    cfg.bus_shared = true;
    _panel_instance.config(cfg);
  }

  setPanel(&_panel_instance);
}

void Display::begin() {
  gfx_.init();
  gfx_.setRotation(1); // landscape 480x320
  gfx_.setColorDepth(16);
  gfx_.fillScreen(0x0000);
  // Backlight via LEDC (ESP32 PWM)
  const int kBlChannel = 0;
  const int kBlFreq = 20000; // 20 kHz to avoid audible noise
  const int kBlResBits = 8;
  ledcSetup(kBlChannel, kBlFreq, kBlResBits);
  ledcAttachPin(tftBl, kBlChannel);
  ledcWrite(kBlChannel, 255);
  buildMenus();
  draw();
}

void Display::loop() {
}

void Display::onRotate(int delta) { menu_.rotate(delta); draw(); }

void Display::onClick() { menu_.confirm(); draw(); }

void Display::onLongPress() { menu_.back(); draw(); }

void Display::updateTelemetry(const ktsu::racebox::ble::RaceboxData& data) {
  lastData_ = data;
  draw();
}

void Display::draw() {
  gfx_.startWrite();
  gfx_.fillScreen(0x0000);
  gfx_.setTextColor(0xFFFF);
  gfx_.setTextSize(2);
  gfx_.setCursor(10, 10);
  gfx_.printf("Speed: %.1f km/h", lastData_.speedKmh);
  gfx_.setCursor(10, 40);
  gfx_.printf("Alt: %.1f m", lastData_.altitudeM);
  gfx_.setCursor(10, 70);
  gfx_.printf("Sats: %lu", (unsigned long)lastData_.sats);

  drawMenu();
  gfx_.endWrite();
}

void Display::drawMenu() {
  using ktsu::racebox::ui::Menu;
  const Menu* m = menu_.currentMenu();
  int y = 110;
  if (m && m->title) {
    gfx_.setTextColor(0x07FF); // cyan title
    gfx_.setCursor(10, y);
    gfx_.print(m->title);
    y += 24;
  }
  if (!m) return;
  for (int i = 0; i < m->itemCount; ++i) {
    bool sel = (i == menu_.currentIndex());
    gfx_.setTextColor(sel ? 0xFFE0 /* yellow */ : 0x7BEF /* grey */);
    gfx_.setCursor(10, y);
    gfx_.print(m->items[i].label ? m->items[i].label : "");
    y += 28;
  }
}

// Static menu definitions (labels and structure). Actions optional.
namespace {
  using namespace ktsu::racebox::ui;

  // Forward declare menus so we can reference by pointer in items
  static Menu rootMenu;
  static Menu settingsMenu;
  static Menu unitsMenu;

  static MenuItem rootItems[3];
  static MenuItem settingsItems[3];
  static MenuItem unitsItems[2];

  void initMenusOnce() {
    // Units submenu
    unitsItems[0].label = "km/h";
    unitsItems[0].submenu = nullptr;
    unitsItems[0].action = MenuAction{};
    unitsItems[1].label = "mph";
    unitsItems[1].submenu = nullptr;
    unitsItems[1].action = MenuAction{};
    unitsMenu.title = "Speed Units";
    unitsMenu.items = unitsItems;
    unitsMenu.itemCount = 2;

    // Settings submenu
    settingsItems[0].label = "Brightness";
    settingsItems[0].submenu = nullptr;
    settingsItems[0].action = MenuAction{};
    settingsItems[1].label = "Units";
    settingsItems[1].submenu = &unitsMenu;
    settingsItems[1].action = MenuAction{};
    settingsItems[2].label = "About";
    settingsItems[2].submenu = nullptr;
    settingsItems[2].action = MenuAction{};
    settingsMenu.title = "Settings";
    settingsMenu.items = settingsItems;
    settingsMenu.itemCount = 3;

    // Root menu
    rootItems[0].label = "Start";
    rootItems[0].submenu = nullptr;
    rootItems[0].action = MenuAction{};
    rootItems[1].label = "Lap";
    rootItems[1].submenu = nullptr;
    rootItems[1].action = MenuAction{};
    rootItems[2].label = "Settings";
    rootItems[2].submenu = &settingsMenu;
    rootItems[2].action = MenuAction{};
    rootMenu.title = "Main Menu";
    rootMenu.items = rootItems;
    rootMenu.itemCount = 3;
  }
}

void Display::buildMenus() {
  static bool initialized = false;
  if (!initialized) { initMenusOnce(); initialized = true; }
  menu_.setRoot(&rootMenu);
}


