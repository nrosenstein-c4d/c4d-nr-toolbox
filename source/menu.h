// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.

#pragma once
#include <stdexcept>
#include <c4d.h>
#include <c4d_apibridge.h>

namespace menu {

/*!
 * Represents a menu entry: Either a plugin ID which will be replaced
 * by the respective plugin's entry or a submenu.
 */
struct MenuEntry {

  enum class Type {
    Submenu,
    Plugin,
    Separator
  };

  Type type;
  String name;
  Int32 id;
  maxon::BaseArray<MenuEntry*> children;

  MenuEntry(MenuEntry const&) = delete;

  inline MenuEntry() : type(Type::Separator), id(0) { }

  inline MenuEntry(Type _type, String const& _name, Int32 _id) : type(_type), name(_name), id(_id) { }

  inline MenuEntry(MenuEntry&& that) = delete; //: type(that.type), name(std::move(that.name)), id(that.id) { }

  ~MenuEntry() {
    for (MenuEntry*& child : this->children) {
      delete child;
      child = nullptr;
    }
    this->children.Flush();
  }

  inline MenuEntry& AddSubmenu(Int32 string_id) {
    String name = GeLoadString(string_id);
    for (MenuEntry* child : this->children) {
      if (child->type == Type::Submenu && child->id == string_id)
        return *child;
    }
    iferr (MenuEntry* e = this->children.Append(new MenuEntry(Type::Submenu, name, string_id)))
      throw std::runtime_error("children.Append() failed");
    return *e;
  }

  inline MenuEntry& AddPlugin(Int32 plugin_id) {
    for (MenuEntry* child : this->children) {
      if (child->type == Type::Plugin && child->id == plugin_id)
        return *child;
    }
    iferr (MenuEntry* e = this->children.Append(new MenuEntry(Type::Plugin, "", plugin_id)))
      throw std::runtime_error("children.Append() failed");
    return *e;
  }

  inline MenuEntry& AddSeparator() {
    MenuEntry** last = this->children.GetLast();
    if (last && (*last)->type == Type::Separator) {
      return **last;
    }
    iferr (MenuEntry* e = this->children.Append(new MenuEntry(Type::Separator, "", -1)))
      throw std::runtime_error("children.Append() failed");
    return *e;
  }

  template <typename... T>
  inline MenuEntry& AddPlugin(Int32 string_id, T&&... remainder) {
    return this->AddSubmenu(string_id).AddPlugin(std::forward<T>(remainder)...);
  }

  template <typename... T>
  inline MenuEntry& AddSeparator(Int32 string_id, T&&... remainder) {
    return this->AddSubmenu(string_id).AddSeparator(std::forward<T>(remainder)...);
  }

  inline BaseContainer CreateMenu(Int32 d = 0) {
    CriticalAssert(type == Type::Submenu);
    BaseContainer bc;
    bc.InsData(MENURESOURCE_SUBTITLE, this->name);
    for (MenuEntry* child : this->children) {
      if (child->type == Type::Submenu) {
        bc.InsData(MENURESOURCE_SUBMENU, child->CreateMenu(d+1));
      }
      else if (child->type == Type::Separator) {
        bc.InsData(MENURESOURCE_SEPERATOR, 1);
      }
      else if (child->type == Type::Plugin) {
        bc.InsData(MENURESOURCE_COMMAND, "PLUGIN_CMD_" + String::IntToString(child->id));
      }
    }

    return bc;
  }

};

MenuEntry& root();
void flush();

} // namespace menu
