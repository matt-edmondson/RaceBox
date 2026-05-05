// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include "../ble/RaceBoxClient.hpp"
#include "Menu.hpp"

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
  void buildMenus();
};

} } } // namespaces


