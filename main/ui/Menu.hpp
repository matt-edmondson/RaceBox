// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <cstddef>
#include <functional>
#include <vector>

namespace ktsu { namespace racebox { namespace ui {

class Menu;

using MenuAction = std::function<void()>;

struct MenuItem {
  const char* label = nullptr;
  const Menu* submenu = nullptr;
  MenuAction action; // optional
};

class Menu {
 public:
  const char* title = nullptr;
  const MenuItem* items = nullptr;
  int itemCount = 0;
};

class MenuNavigator {
 public:
  MenuNavigator() = default;
  explicit MenuNavigator(const Menu* root) { setRoot(root); }

  void setRoot(const Menu* root);
  void rotate(int delta);
  void confirm();
  void back();

  const Menu* currentMenu() const;
  int currentIndex() const;
  // The item under the cursor, or nullptr when there is no valid selection.
  const MenuItem* currentItem() const;
  bool canGoBack() const { return frames_.size() > 1; }
  // Nesting depth: 1 at the root, 2 inside a submenu, and so on.
  size_t depth() const { return frames_.size(); }

 private:
  struct Frame {
    const Menu* menu = nullptr;
    int index = 0;
  };

  std::vector<Frame> frames_;
};

} } } // namespaces


