// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "Display.hpp"
#include "Menu.hpp"

#include "../common/IdfCompat.hpp"

using namespace ktsu::racebox::ui;

static const char* TAG = "Display";

void Display::begin() {
  ESP_LOGI(TAG, "Display init (stub)");
  buildMenus();
  draw();
}

void Display::loop() {
}

void Display::onRotate(int delta) { menu_.rotate(delta); draw(); }

void Display::onClick() { menu_.confirm(); draw(); }

void Display::onLongPress() { menu_.back(); draw(); }

void Display::updateTelemetry(const ktsu::racebox::ble::RaceboxData& data) { lastData_ = data; draw(); }

void Display::draw() {
  // TODO: integrate real graphics (e.g., LVGL or ILI9488 driver) under IDF
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


