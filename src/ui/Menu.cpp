// Copyright (c) Matthew Edmondson, 2025
// All rights reserved.
// Licensed under the MIT license.

#include "Menu.hpp"

using namespace ktsu::racebox::ui;

void MenuNavigator::setRoot(const Menu* root) {
  frames_.clear();
  if (root) {
    Frame f;
    f.menu = root;
    f.index = 0;
    frames_.push_back(f);
  }
}

void MenuNavigator::rotate(int delta) {
  if (frames_.empty()) return;
  Frame& f = frames_.back();
  if (!f.menu || f.menu->itemCount <= 0) return;
  int count = f.menu->itemCount;
  int idx = f.index + delta;
  // wrap around
  while (idx < 0) idx += count;
  while (idx >= count) idx -= count;
  f.index = idx;
}

void MenuNavigator::confirm() {
  if (frames_.empty()) return;
  Frame& f = frames_.back();
  if (!f.menu || f.menu->itemCount <= 0) return;
  const MenuItem& item = f.menu->items[f.index];
  if (item.submenu) {
    Frame nf;
    nf.menu = item.submenu;
    nf.index = 0;
    frames_.push_back(nf);
    return;
  }
  if (item.action) { item.action(); }
}

void MenuNavigator::back() {
  if (frames_.size() > 1) { frames_.pop_back(); }
}

const Menu* MenuNavigator::currentMenu() const {
  if (frames_.empty()) return nullptr;
  return frames_.back().menu;
}

int MenuNavigator::currentIndex() const {
  if (frames_.empty()) return 0;
  return frames_.back().index;
}


