// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.

#include "menu.h"
#include <NiklasRosenstein/c4d/cleanup.hpp>

namespace nr { using namespace niklasrosenstein; }

static menu::MenuEntry* _root = nullptr;

menu::MenuEntry& menu::root() {
  if (!_root) {
    _root = new MenuEntry(MenuEntry::Type::Submenu, "nr-toolbox", 0);
    nr::c4d::cleanup(&flush);
  }
  CriticalAssert(_root != nullptr);
  return *_root;
}

void menu::flush() {
  delete _root;
  _root = nullptr;
}
