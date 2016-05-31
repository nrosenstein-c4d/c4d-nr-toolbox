/**
 * Copyright (c) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * @file nr/depmanager.cpp
 * @brief Implementation of the public Dependency Manager API.
 */

#include "depmanager.h"

namespace nr {
namespace depmanager {

DependencyTypeData::DependencyTypeData() : super() { }

DependencyTypeData::~DependencyTypeData() { }

Bool DependencyTypeData::AcceptDependency(BaseList2D* element, BaseList2D* pot_dependency) {
  return true;
}

Int32 DependencyTypeData::GetDefaultInExFlags(BaseList2D* element, BaseList2D* pot_dependency) {
  return 0;
}

Bool DependencyTypeData::RemoveElement(BaseList2D* element, BaseList2D* parent) {
  if (element && parent) {
    BaseDocument* doc = element->GetDocument();
    GeData ge_list;
    if (GetElementDependencies(parent, ge_list)) {
      InExcludeData* list = GetInExcludeData(ge_list);
      if (list) {
        Int32 index = list->GetObjectIndex(doc, element);
        if (index >= 0) {
          list->DeleteObject(doc, element);
          if (doc) doc->AddUndo(UNDOTYPE_CHANGE_SMALL, parent);
          SetElementDependencies(parent, ge_list);
          return true;
        }
      }
    }
  }
  else if (element && !parent) {
    BaseDocument* doc = element->GetDocument();
    if (doc) doc->AddUndo(UNDOTYPE_DELETE, element);
    element->Remove();
    BaseList2D::Free(element);
    return true;
  }
  return false;
}

Bool DependencyTypeData::Message(BaseList2D* element, BaseList2D* parent, Int32 type, void* p_data) {
  switch (type) {
    case DEPMSG_GETICON:
      if (element && p_data) {
        ((MsgGetIcon*) p_data)->caller_is_owner = false;
        element->GetIcon(&((MsgGetIcon*) p_data)->icon);
      }
      break;
  }
  return true;
}

void* DependencyTypeData::SelectionStart(BaseDocument* doc, Int32 mode) {
  return nullptr;
}

Bool DependencyTypeData::SelectElement(BaseList2D* element, BaseList2D* parent, Int32 mode, void* userdata) {
  return false; // the caller should handle the event
}

void DependencyTypeData::SelectionEnd(BaseDocument* doc, void* userdata) {
}


void GetDependencyTypePlugins(AtomArray* arr, Bool sort) {
  if (!arr) return;
  FilterPluginList(*arr, PLUGINTYPE_DEPENDENCYTYPE, sort);
}

Bool FillDependencyTypePlugin(DEPENDENCYTYPEPLUGIN* plugin, Int32 info, BaseData* adr) {
  if (!plugin) return false;

  // BASEPLUGIN
  plugin->info = info;
  plugin->Destructor = &DependencyTypeData::Destructor;

  // STATICPLUGIN
  plugin->adr = adr;

  // DEPENDENCYTYPEPLUGIN
  plugin->GetFirstElement         = &DependencyTypeData::GetFirstElement;
  plugin->AcceptDependency        = &DependencyTypeData::AcceptDependency;
  plugin->GetDefaultInExFlags     = &DependencyTypeData::GetDefaultInExFlags;
  plugin->GetElementDependencies  = &DependencyTypeData::GetElementDependencies;
  plugin->SetElementDependencies  = &DependencyTypeData::SetElementDependencies;
  plugin->GetDisplayFlags         = &DependencyTypeData::GetDisplayFlags;
  plugin->RemoveElement           = &DependencyTypeData::RemoveElement;

  plugin->Message                 = &DependencyTypeData::Message;
  plugin->SelectionStart          = &DependencyTypeData::SelectionStart;
  plugin->SelectElement           = &DependencyTypeData::SelectElement;
  plugin->SelectionEnd            = &DependencyTypeData::SelectionEnd;
  return true;
}

Bool RegisterDependencyType(Int32 pluginid, Int32 info, const String& name, DependencyTypeData* plugin) {
  if (!plugin) return false;

  DEPENDENCYTYPEPLUGIN data;
  ClearMem(&data, sizeof(data));
  if (!FillDependencyTypePlugin(&data, info, plugin)) return false;

  // TODO: Fix the crash when exiting Cinema
  return GeRegisterPlugin(PLUGINTYPE_DEPENDENCYTYPE, pluginid, name, &data, sizeof(data));
}

}} // namespace nr::depmanager
