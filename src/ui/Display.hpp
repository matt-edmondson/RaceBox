// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <LovyanGFX.hpp>
#include "ble/RaceBoxClient.hpp"
#include "ui/Menu.hpp"

namespace ktsu { namespace racebox { namespace ui {

class LGFX : public lgfx::LGFX_Device {
 public:
  LGFX();
 private:
  lgfx::Panel_ILI9488 _panel_instance; // ILI9488 SPI panel
  lgfx::Bus_SPI _bus_instance;
};

class Display {
 public:
  void begin();
  void loop();
  void onRotate(int delta);
  void onClick();
  void onLongPress();
  void updateTelemetry(const ktsu::racebox::ble::RaceboxData& data);

 private:
  LGFX gfx_;
    ktsu::racebox::ui::MenuNavigator menu_{};
  ktsu::racebox::ble::RaceboxData lastData_{};
  void draw();
    void drawMenu();
    void buildMenus();
};

} } } // namespaces


