// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.

#include "menu.h"
#include <NiklasRosenstein/c4d/cleanup.hpp>

namespace nr { using namespace niklasrosenstein; }

static menu::entry* _root = nullptr;

menu::entry& menu::root() {
  if (!_root) {
    _root = NewObjClear(menu::entry, "nr-toolbox", 0);
    nr::c4d::cleanup(&menu::flush);
  }
  CriticalAssert(_root != nullptr);
  return *_root;
}

void menu::flush() {
  DeleteObj(_root);
}
