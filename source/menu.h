// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.

#pragma once
#include <stdexcept>
#include <c4d.h>

namespace menu {

/*!
 * Represents a menu entry: Either a plugin ID which will be replaced
 * by the respective plugin's entry or a submenu. In the case of a
 * submenu, the #plugin_id equals 0 and the entry has no #children and
 * no #name.
 */
struct entry {
  String name;
  Int32 plugin_id;
  maxon::BaseArray<entry> children;

  entry(entry const&) = delete;

  inline entry() : plugin_id(0) { }

  inline entry(String const& _name, Int32 _plugin_id)
  : name(_name), plugin_id(_plugin_id) { }

  inline entry(entry&& that)
  : name(std::move(that.name)), plugin_id(that.plugin_id) { }

  inline entry& AddSubmenu(Int32 string_id, Bool create=true) {
    String name = GeLoadString(string_id);
    for (entry& child : this->children) {
      if (child.name == name)
        return child;
    }
    ifnoerr (entry& e = this->children.Append(entry(name, 0)))
      return e;
    throw std::runtime_error("children.Append() failed");
  }

  inline entry& AddPlugin(Int32 plugin_id) {
    for (entry& child : this->children) {
      if (child.plugin_id == plugin_id)
        return child;
    }
    ifnoerr (entry& e = this->children.Append(entry("", plugin_id)))
      return e;
    throw std::runtime_error("children.Append() failed");
  }

  template <typename... T>
  inline entry& AddPlugin(Int32 string_id, T&&... remainder) {
    return this->AddSubmenu(string_id).AddPlugin(std::forward<T>(remainder)...);
  }

  inline entry& AddSeparator() {
    entry* last = this->children.GetLast();
    if (last && last->plugin_id == -1) {
      return *last;
    }
    ifnoerr (entry& e = this->children.Append(entry("", -1)))
      return e;
    throw std::runtime_error("children.Append() failed");
  }

  template <typename... T>
  inline entry& AddSeparator(Int32 string_id, T&&... remainder) {
    return this->AddSubmenu(string_id).AddSeparator(std::forward<T>(remainder)...);
  }

  inline BaseContainer CreateMenu() {
    BaseContainer bc;
    bc.InsData(MENURESOURCE_SUBTITLE, this->name);
    for (entry& child : this->children) {
      if (child.plugin_id == 0) {
        bc.InsData(MENURESOURCE_SUBMENU, child.CreateMenu());
      }
      else if (child.plugin_id == -1) {
        bc.InsData(MENURESOURCE_SEPERATOR, 1);
      }
      else {
        bc.InsData(MENURESOURCE_COMMAND,
          "PLUGIN_CMD_" + String::IntToString(child.plugin_id));
      }
    }
    return bc;
  }

};

entry& root();
void flush();

} // namespace menu
