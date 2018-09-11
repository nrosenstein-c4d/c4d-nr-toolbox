// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.

#include <map>
#include <fstream>

#include <c4d.h>
#include <dlib/config_reader.h>
#include <NiklasRosenstein/c4d/cleanup.hpp>
#include <NiklasRosenstein/c4d/raii.hpp>

#include "config.h"
#include "menu.h"

using config_reader = dlib::config_reader;
using config_reader_access_error = config_reader::config_reader_access_error;
using niklasrosenstein::c4d::auto_string;
using niklasrosenstein::c4d::auto_bitmap;

static std::string filename;
static config_reader cfg;
static std::map<std::string, bool> features;


bool nr::config::init_config(Filename const& fn, String* error) {
  ::filename = auto_string(fn);
  try {
    cfg.load_from(::filename);
    return true;
  }
  catch (std::exception& exc) {
    if (error) *error = exc.what();
    return false;
  }
}


bool nr::config::check_feature(String const& feature_name) {
  std::string const name = auto_string(feature_name).get();
  try {
    return ::features.at(name);
  }
  catch (std::out_of_range&) {
    // intentionally left blank
  }

  bool result = true;
  try {
    std::string value = cfg.block("features")[name];
    result = (value == "true");
  }
  catch (config_reader_access_error&) {
    // intentionally left blank
  }
  ::features[name] = result;
  return result;
}


void nr::config::enable_feature(String const& feature_name, Bool enabled) {
  std::string const name = auto_string(feature_name).get();
  features[name] = enabled;
}


bool nr::config::save_config(Filename const* fn) {
  std::string outfile = ::filename;
  if (fn) outfile = auto_string(*fn);
  std::ofstream stream(outfile);
  if (!stream) return false;
  stream << "features {\n";
  for (auto& entry : ::features) {
    stream << "  " << entry.first << " = ";
    if (entry.second) stream << "true\n";
    else stream << "false\n";
  }
  stream << "}\n";
  stream.close();
  return true;
}

//============================================================================

enum {
  PLUGIN_ID = 1037773,
  EDT_CHECKBOX_START = 1000
};

struct dialog : public GeDialog {

  Bool CreateLayout() override {
    SetTitle("Feature Manager"_s);
    GroupBegin(0, BFH_SCALEFIT | BFV_SCALEFIT, 1, 0, ""_s, 0); {
      AddStaticText(0, BFH_CENTER, 0, 0, "Cinema 4D needs to be restarted for changes to take effect."_s, 0);
      GroupBorderSpace(4, 4, 4, 4);
      GroupBorderSpace(4, 4, 4, 4);

      Int32 index = 0;
      for (auto& entry : ::features) {
        AddCheckbox(EDT_CHECKBOX_START + index++, BFH_LEFT, 0, 0, String(entry.first.c_str()));
      }
      GroupEnd();
    }
    AddDlgGroup(DLG_OK | DLG_CANCEL);
    return true;
  }

  Bool InitValues() override {
    Int32 index = 0;
    for (auto& entry : ::features) {
      SetBool(EDT_CHECKBOX_START + index++, entry.second);
    }
    return true;
  }

  Bool Command(Int32 param, BaseContainer const& bc) override {
    switch (param) {
      case DLG_OK:
        this->save();
        // SWITCH FALLTHROUGH
      case DLG_CANCEL:
        this->Close();
        return true;
    }
    return false;
  }

private:

  void save() {
    Int32 index = 0;
    for (auto& entry : ::features) {
      Bool enabled = true;
      this->GetBool(EDT_CHECKBOX_START + index++, enabled);
      entry.second = enabled;
    }
    if (nr::config::save_config()) {
      MessageDialog("Configuration saved. You need to restart C4D for the "
        "changes to take effect.");
    }
    else {
      MessageDialog("Configuration could not be saved.");
    }
  }

};

struct command : public CommandData {
  dialog dlg;
  Bool Execute(BaseDocument* doc) override {
    return dlg.Open(DLG_TYPE_ASYNC, PLUGIN_ID, -1, -1, 300, 200);
  }
};

//============================================================================

Bool RegisterFeatureManager() {
  menu::root().AddPlugin(PLUGIN_ID);
  Int32 const info = PLUGINFLAG_HIDEPLUGINMENU;
  auto_bitmap icon("res/icons/feature_manager.png");
  String const help = "";
  return RegisterCommandPlugin(
    PLUGIN_ID,
    "Feature Manager"_s,
    info,
    icon,
    help,
    NewObjClear(command));
}
