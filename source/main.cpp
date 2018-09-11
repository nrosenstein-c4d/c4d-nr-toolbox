// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.

#include <c4d.h>
#include <c4d_apibridge.h>
#include <NiklasRosenstein/c4d/raii.hpp>
#include <NiklasRosenstein/c4d/cleanup.hpp>
#include "res/c4d_symbols.h"
#include "misc/print.h"
#include "config.h"
#include "menu.h"
#include "GIT_VERSION.h"

namespace nr { using namespace niklasrosenstein; }

#define _REGISTER(x)\
  do {\
    extern Bool Register##x();\
    if (Register##x()) {\
      print::info("  " #x " registered.");\
    }\
    else {\
      print::info("  " #x " could not be registered.");\
    }\
  } while (0)

#define _MESSAGE(x, msg, pdata)\
  ([msg, pdata] {\
    extern Bool Message##x(Int32, void*);\
    return Message##x(msg, pdata);\
  })()

//============================================================================
//============================================================================
Bool PluginStart() {
  print::info("=============================================================");
  print::info("nr-toolbox v" GIT_VERSION " -- Developed by Niklas Rosenstein");

  String err;
  Bool ok = nr::config::init_config(GeGetC4DPath(C4D_PATH_PREFS) + "nr_toolbox_features.cfg", &err);
  if (!ok) {
    ok = nr::config::init_config(GeGetPluginPath() + "default_features.cfg", &err);
  }
  if (!ok) {
    GePrint("Error: Could not read configuration file.");
    GePrint(err);
  }

  #ifdef HAVE_COMMANDS
  if (nr::config::check_feature("commands")) {
    _REGISTER(ExplodeCommand);
    _REGISTER(ResolveDuplicates);
    menu::root().AddSeparator();
  }
  #endif

  #ifdef HAVE_COLORPALETTE
  if (nr::config::check_feature("colorpalette")) {
    _REGISTER(ColorPalette);
    _REGISTER(SwatchShader);
  }
  #endif

  #ifdef HAVE_AUTOCONNECT
  if (nr::config::check_feature("autoconnect")) {
    _REGISTER(AutoConnect);
  }
  #endif

  #ifdef HAVE_DEPMANAGER
  if (nr::config::check_feature("dependency_manager")) {
    _REGISTER(DependencyManager);
    _REGISTER(DependencyTypeExtensions);
    menu::root().AddSeparator();
  }
  #endif

  #ifdef HAVE_NRWORKFLOW
  if (nr::config::check_feature("workflowtools")) {
    _REGISTER(NRWorkflow);
  }
  #endif

  #ifdef HAVE_PR1MITIVE
  if (nr::config::check_feature("pr1mitive")) {
    _REGISTER(Pr1mitive);
  }
  #endif

  #ifdef HAVE_PROCEDURAL
  if (nr::config::check_feature("procedural")) {
    _REGISTER(Procedural);
  }
  #endif

  #ifdef HAVE_SMEARDEFORMER
  if (nr::config::check_feature("smeardeformer")) {
    _REGISTER(SmearDeformer);
  }
  #endif

  #ifdef HAVE_WEBPIO
  if (nr::config::check_feature("webp")) {
    _REGISTER(WebpIO);
  }
  #endif

  #ifdef HAVE_XPE
  if (nr::config::check_feature("xpressoeffector")) {
    _REGISTER(XPE);
  }
  #endif

  #ifdef HAVE_TEAPRESSO
  if (nr::config::check_feature("teapresso")) {
    _REGISTER(Teapresso);
  }
  #endif

  menu::root().AddSeparator();
  _REGISTER(FeatureManager);

  print::info("=============================================================");
  return true;
}

//============================================================================
//============================================================================
void PluginEnd() {
  nr::c4d::do_cleanup();
}

//============================================================================
//============================================================================
Bool PluginMessage(Int32 msg, void* pdata) {
  GePrint(">>>>> nrtoolbox loaded\n"_s);
  switch (msg) {
    case C4DPL_INIT_SYS:
      c4d_apibridge::GlobalResource().Init();
      break;
    case C4DPL_BUILDMENU: {
      BaseContainer* bc = GetMenuResource("M_EDITOR"_s);
      if (bc) {
        bc->InsData(MENURESOURCE_SUBMENU, menu::root().CreateMenu());
        menu::flush();
      }
      break;
    }
  }
  #ifdef HAVE_PR1MITIVE
    if (!_MESSAGE(Pr1mitive, msg, pdata)) return false;
  #endif
  #ifdef HAVE_TEAPRESSO
    if (!_MESSAGE(Teapresso, msg, pdata)) return false;
  #endif
  return true;
}
