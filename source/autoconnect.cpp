/**
 * coding: utf-8
 *
 * Copyright (C) 2013-2014, Niklas Rosenstein
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "c4d.h"
#include "c4d_apibridge.h"
#include "maxon/sort.h"
#include "customgui_inexclude.h"
#include "customgui_hyperlink.h"
#include "lib_activeobjectmanager.h"

#include "res/c4d_symbols.h"
#include "menu.h"
#include <NiklasRosenstein/c4d/raii.hpp>

namespace nr { using namespace niklasrosenstein; }

#define ID_AUTOCONNECT_COMMAND 1030516

#define ID_SPRING 180000010
#define ID_CONNECTOR 180000011
#define ID_MOTOR 180000012
#define ID_RIGIDBODY 180000102
#define FORCE_TYPE 12071
#define FORCE_OBJECT_A 12001
#define FORCE_OBJECT_B 12002
#define RIGID_BODY_HIERARCHY 11006
#define RIGID_BODY_HIERARCHY_INHERIT 1
#define RIGID_BODY_HIERARCHY_COMPOUND 2

struct Connection
{
  BaseObject* obj1, *obj2;
  Float delta;

  Connection(BaseObject* obj1, BaseObject* obj2)
  : obj1(obj1), obj2(obj2)
  {
    delta = (obj1->GetMg().off - obj2->GetMg().off).GetLength();
  }

  Connection(BaseObject* obj1, BaseObject* obj2, Float delta)
  : obj1(obj1), obj2(obj2), delta(delta)
  { }
};

class ConnectionList : public maxon::BaseArray<Connection>
{
public:

  bool HasConnection(BaseObject* obj1, BaseObject* obj2)
  {
    Iterator it = Begin();
    const Iterator it_end = End();
    for (; it != it_end; ++it)
    {
      bool result = it->obj1 == obj1 && it->obj2 == obj2;
      result = result || (it->obj2 == obj1 && it->obj1 == obj2);
      if (result) return true;
    }
    return false;
  }

  void SortByDelta()
  {
    struct Sorter : public maxon::BaseSort<Sorter>
    {
      static inline
      Bool LessThan(const Connection& a, const Connection& b)
      {
        return a.delta < b.delta;
      }
    };
    Sorter sorter;
    sorter.Sort(Begin(), End());
  }
};

struct ConnectOptions
{
  Int32 forcePluginId, forceType;
  Int32 connectMode, maxConnections;
  Float radius;
  Bool closedChain;

  String forceName;
  maxon::BaseArray<BaseObject*> output;
};

Bool ConnectObjects(
    String prefix, const maxon::BaseArray<BaseObject*>& objects,
    ConnectOptions& options)
{

  // Check if the object can successfully be allocatd for the
  // specified plugin id.
  BaseObject* test = BaseObject::Alloc(options.forcePluginId);
  if (!test)
  {
    GeDebugOut("Can not allocate plugin object " + String::IntToString(options.forcePluginId));
    return false;
  }

  // Get the name of the object so we can use it later.
  options.forceName = test->GetName();
  BaseObject::Free(test);
  if (c4d_apibridge::IsEmpty(prefix))
    prefix = options.forceName + ": ";

  // Final result of the function.
  Bool success = true;

  // Create the ConnectionList for the objects.
  ConnectionList list;
  switch (options.connectMode)
  {
    case IDS_AUTOCONNECT_CMB_MODE_ALL:
    {
      const auto end = objects.End();
      for (auto it=objects.Begin(); it != end; ++it)
      {
        for (auto jt=objects.Begin(); jt != end; ++jt)
        {
          if (*it == *jt) continue;
          if (list.HasConnection(*it, *jt)) continue;
          list.Append(Connection(*it, *jt));
        }
      }
      break;
    }
    case IDS_AUTOCONNECT_CMB_MODE_NEIGHBOR:
    {
      const auto end = objects.End();
      for (auto it=objects.Begin(); it != end; ++it)
      {

        // Find the nearest object.
        BaseObject* nearest = nullptr;
        Float delta;
        for (auto jt=objects.Begin(); jt != end; ++jt)
        {
          if (*it == *jt) continue;
          if (list.HasConnection(*it, *jt)) continue;

          Connection conn(*it, *jt);
          if (!nearest || conn.delta < delta)
          {
            nearest = *jt;
            delta = conn.delta;
          }
        }

        if (nearest)
          list.Append(Connection(*it, nearest));
      }
      break;
    }
    case IDS_AUTOCONNECT_CMB_MODE_CHAIN:
    {
      options.radius = 0;
      options.maxConnections = 0;
      const auto first = objects.Begin();
      for (auto it=objects.Begin(); it != objects.End(); ++it)
      {
        Bool isLast = (it + 1) == objects.End();
        if (isLast && (options.closedChain && *it != *first))
        {
          Connection conn(*it, *first);
          list.Append(conn);
        }
        else if (!isLast)
        {
          Connection conn(*it, *(it + 1));
          list.Append(conn);
        }
      }
      break;
    }
    default:
      GeDebugOut(String(__FUNCTION__) + ": Invalid connectMode");
      return false;
  }

  // Sort the list by distance.
  list.SortByDelta();

  // This map contains the number of connections each
  // object already has to make sure no object exceeds the
  // maximum number if connections.
  maxon::Bool created = false;
  c4d_apibridge::HashMap<C4DAtom*, Int32> map;

  // Iterate over all connections and establish them.
  const auto end = list.End();
  for (auto it=list.Begin(); it != end; ++it)
  {
    if (options.radius > 0.000001 && it->delta > options.radius)
      continue;

    // Find and eventually initialize the entries in the map.
    auto entry1 = map.FindOrCreateEntry(it->obj1, created);
    if (created) entry1->GetValue() = 0;
    auto entry2 = map.FindOrCreateEntry(it->obj2, created);
    if (created) entry2->GetValue() = 0;

    // Determine if the maximum number of connections is reached.
    const Int32 nConn1 = entry1->GetValue();
    const Int32 nConn2 = entry2->GetValue();
    if (options.maxConnections > 0 &&
        (nConn1 >= options.maxConnections || nConn2 >= options.maxConnections))
      continue;

    // Create the force object.
    BaseObject* force = BaseObject::Alloc(options.forcePluginId);
    if (!force)
    {
      success = false;
      break;
    }

    // Update the parameters.
    force->SetParameter(FORCE_TYPE, options.forceType, DESCFLAGS_SET_0);
    force->SetParameter(FORCE_OBJECT_A, it->obj1, DESCFLAGS_SET_0);
    force->SetParameter(FORCE_OBJECT_B, it->obj2, DESCFLAGS_SET_0);
    force->SetBit(BIT_ACTIVE);
    force->SetName(prefix + it->obj1->GetName() + " - " + it->obj2->GetName());
    ++entry1->GetValue();
    ++entry2->GetValue();

    options.output.Append(force);

    // Position the force object in between the two objects.
    Matrix mg;
    mg.off = (it->obj1->GetMg().off + it->obj2->GetMg().off) * 0.5;
    force->SetMl(mg);
  }

  return true;
}

class AutoConnectDialog : public GeDialog
{
  typedef GeDialog super;
public:
  AutoConnectDialog()
  : super() { }

  Bool ExecuteAutoConnect()
  {
    ConnectOptions options;
    Bool addDynamicsTag = false, inheritDynamicsTag = true;
    GetInt32(IDS_AUTOCONNECT_CMB_FORCE, options.forcePluginId);
    GetInt32(IDS_AUTOCONNECT_CMB_TYPE, options.forceType);
    GetInt32(IDS_AUTOCONNECT_CMB_MODE, options.connectMode);
    GetInt32(IDS_AUTOCONNECT_EDT_MAXCONN, options.maxConnections);
    GetFloat(IDS_AUTOCONNECT_EDT_RADIUS, options.radius);
    GetBool(IDS_AUTOCONNECT_CHK_CLOSED, options.closedChain);
    GetBool(IDS_AUTOCONNECT_CHK_ADDDYNAMICS, addDynamicsTag);
    GetBool(IDS_AUTOCONNECT_CHK_COMPOUND, inheritDynamicsTag);

    // Create an InExcludeData for the selection object.
    GeData ge_selection(CUSTOMGUI_INEXCLUDE_LIST, DEFAULTVALUE);
    auto selectionList = static_cast<InExcludeData*>(
      ge_selection.GetCustomDataType(CUSTOMGUI_INEXCLUDE_LIST));
    if (!selectionList)
      return false;

    // Get the active document and object.
    BaseDocument* doc = GetActiveDocument();
    if (!doc)
      return false;
    BaseObject* op = doc->GetActiveObject();
    if (!op)
      return false;

    // Create the root object that will contain the connectors.
    AutoFree<BaseObject> root(BaseObject::Alloc(Onull));
    if (!root)
      return false;

    // Function to create a dynamics tag.
    auto fAddDynamicsTag = [doc] (BaseObject* op, Int32 mode)
    {
      // Create a dynamics tag for the root object if it
      // does not already exist.
      BaseTag* dyn = op->GetTag(ID_RIGIDBODY);
      if (!dyn)
      {
        dyn = op->MakeTag(ID_RIGIDBODY);
        if (dyn) doc->AddUndo(UNDOTYPE_NEW, dyn);
      }

      // Update the parameters.
      if (dyn)
      {
        dyn->SetParameter(RIGID_BODY_HIERARCHY, mode, DESCFLAGS_SET_0);
        doc->AddUndo(UNDOTYPE_CHANGE_SMALL, dyn);
      }
    };

    // This list will contain all objects that should be connected.
    // While collecting, create the dynamics tags.
    maxon::BaseArray<BaseObject*> objects;
    doc->StartUndo();
    for (BaseObject* child=op->GetDown(); child; child=child->GetNext())
    {
      objects.Append(child);
      if (addDynamicsTag && inheritDynamicsTag)
        fAddDynamicsTag(child, RIGID_BODY_HIERARCHY_COMPOUND);
    }
    if (addDynamicsTag && !inheritDynamicsTag)
      fAddDynamicsTag(op, RIGID_BODY_HIERARCHY_INHERIT);

    // If no objects where collected, quit already.
    if (objects.GetCount() <= 0)
    {
      doc->EndUndo();
      doc->DoUndo(false);
      return true;
    }

    // Create the connection objects.
    ConnectObjects(op->GetName() + ": ", objects, options);

    // Fill the selection list and insert the objects.
    for (auto it=options.output.Begin(); it != options.output.End(); ++it)
    {
      (*it)->InsertUnderLast(root);
      doc->AddUndo(UNDOTYPE_NEW, *it);
      selectionList->InsertObject(*it, 0);
    }

    root->SetName(op->GetName() + ": " + root->GetName() + " (" + options.forceName + ")");
    doc->InsertObject(root, nullptr, nullptr);
    doc->AddUndo(UNDOTYPE_NEW, root);

    // Create the selection object.
    if (selectionList->GetObjectCount() > 0)
    {
      BaseObject* selection = BaseObject::Alloc(Oselection);
      if (selection)
      {
        selection->SetParameter(SELECTIONOBJECT_LIST, ge_selection, DESCFLAGS_SET_0);
        selection->SetName(op->GetName() + ": " + options.forceName + " (" + selection->GetName() + ")");
        doc->InsertObject(selection, nullptr, nullptr);
        doc->AddUndo(UNDOTYPE_NEW, selection);
      }
      ActiveObjectManager_SetMode(ACTIVEOBJECTMODE_OBJECT, false);
      doc->SetActiveObject(selection);
    }
    else
      doc->SetActiveObject(root);

    root.Release();
    doc->EndUndo();
    EventAdd();
    return true;
  }

  //| GeDialog Overrides

  Bool CreateLayout()
  {
    GroupBeginInMenuLine();
    {
      BaseContainer customdata;
      customdata.SetString(HYPERLINK_LINK_TEXT, GeLoadString(IDS_AUTOCONNECT_CMG_DEVLINK));
      customdata.SetString(HYPERLINK_LINK_DEST, GeLoadString(IDS_AUTOCONNECT_CMG_DEVLINK_LINK));
      customdata.SetBool(HYPERLINK_IS_LINK, true);
      customdata.SetBool(HYPERLINK_ALIGN_RIGHT, true);
      customdata.SetBool(HYPERLINK_NO_UNDERLINE, true);
      AddCustomGui(IDS_AUTOCONNECT_CMG_DEVLINK, CUSTOMGUI_HYPER_LINK_STATIC, ""_s,
             0, 0, 0, customdata);
      GroupEnd();
    }
    return LoadDialogResource(IDS_AUTOCONNECT_DLG_MAIN, nullptr, BFH_SCALEFIT | BFV_SCALEFIT);
  }

  Bool InitValues()
  {
    // Initialize the force items.
    FreeChildren(IDS_AUTOCONNECT_CMB_FORCE);
    AddForceCycleElement(ID_SPRING);
    AddForceCycleElement(ID_CONNECTOR);
    AddForceCycleElement(ID_MOTOR);
    SetInt32(IDS_AUTOCONNECT_CMB_FORCE, ID_CONNECTOR);

    SetInt32(IDS_AUTOCONNECT_CMB_TYPE, -1);
    SetInt32(IDS_AUTOCONNECT_CMB_MODE, IDS_AUTOCONNECT_CMB_MODE_ALL);
    SetInt32(IDS_AUTOCONNECT_EDT_MAXCONN, 0, 0, 20, 1, false, 0, LIMIT<Int32>::MAX);
    SetFloat(IDS_AUTOCONNECT_EDT_RADIUS, (Float) 0, 0, 500, 1.0, FORMAT_METER, 0, MAXVALUE_FLOAT, false, false);

    SetBool(IDS_AUTOCONNECT_CHK_ADDDYNAMICS, false);
    SetBool(IDS_AUTOCONNECT_CHK_COMPOUND, false);
    SetBool(IDS_AUTOCONNECT_CHK_CLOSED, false);

    UpdateGadgets(true);
    return super::InitValues();
  }

  Bool Command(Int32 id, const BaseContainer& msg)
  {
    UpdateGadgets();
    switch (id) {
      case IDS_AUTOCONNECT_BTN_EXECUTE:
        ExecuteAutoConnect();
        break;
      case IDS_AUTOCONNECT_CMB_FORCE:
        UpdateGadgets(true);
        break;
    }
    return super::Command(id, msg);
  }

  Bool CoreMessage(Int32 id, const BaseContainer& msg)
  {
    switch (id) {
      case EVMSG_CHANGE:
        UpdateGadgets();
        break;
    }
    return super::CoreMessage(id, msg);
  }
private:

  void UpdateGadgets(Bool relevants=false)
  {
    Int32 mode;
    Bool add_dynamics;
    GetBool(IDS_AUTOCONNECT_CHK_ADDDYNAMICS, add_dynamics);
    GetInt32(IDS_AUTOCONNECT_CMB_MODE, mode);

    BaseDocument* doc = GetActiveDocument();
    BaseObject* op = doc ? doc->GetActiveObject() : nullptr;

    Enable(IDS_AUTOCONNECT_EDT_MAXCONN, mode != IDS_AUTOCONNECT_CMB_MODE_CHAIN);
    Enable(IDS_AUTOCONNECT_EDT_RADIUS, mode != IDS_AUTOCONNECT_CMB_MODE_CHAIN);
    Enable(IDS_AUTOCONNECT_CHK_CLOSED, mode == IDS_AUTOCONNECT_CMB_MODE_CHAIN);
    Enable(IDS_AUTOCONNECT_CHK_COMPOUND, add_dynamics);

    // Need at least two child objects on the active object.
    Bool execEnabled = op != nullptr && op->GetDown() != nullptr
        && op->GetDown()->GetNext() != nullptr;
    Enable(IDS_AUTOCONNECT_BTN_EXECUTE, execEnabled);

    if (relevants)
    {
      FreeChildren(IDS_AUTOCONNECT_CMB_TYPE);

      Int32 pluginid;
      GetInt32(IDS_AUTOCONNECT_CMB_FORCE, pluginid);

      do {
        BaseObject* op = BaseObject::Alloc(pluginid);
        if (!op) break;
        AutoFree<BaseObject> free(op);

        AutoAlloc<Description> desc;
        if (!op->GetDescription(desc, DESCFLAGS_DESC_0)) break;

        BaseContainer temp;
        AutoAlloc<AtomArray> arr;
        const BaseContainer* param = desc->GetParameter(FORCE_TYPE, temp, arr);
        if (!param) break;

        const BaseContainer* cycle = param->GetContainerInstance(DESC_CYCLE);
        if (!cycle) break;

        const BaseContainer* icons = param->GetContainerInstance(DESC_CYCLEICONS);

        Int32 i = 0;
        Int32 last_id = -1;
        while (true) {
          Int32 id = cycle->GetIndexId(i++);
          if (id == NOTOK) break;

          Int32 icon = icons ? icons->GetInt32(id) : -1;

          String name = cycle->GetString(id);
          if (!c4d_apibridge::IsEmpty(name)) {
            if (icon > 0) name += "&i" + String::IntToString(icon);
            if (last_id < 0) last_id = id;
            AddChild(IDS_AUTOCONNECT_CMB_TYPE, id, name);
          }
        }

        SetInt32(IDS_AUTOCONNECT_CMB_TYPE, last_id);
      } while (0);

      LayoutChanged(IDS_AUTOCONNECT_CMB_TYPE);
    }
  }

  Bool AddForceCycleElement(Int32 id)
  {
    BaseObject* obj = BaseObject::Alloc(id);
    if (!obj) return false;

    AddChild(IDS_AUTOCONNECT_CMB_FORCE, id, obj->GetName() + "&i" + String::IntToString(id));
    BaseObject::Free(obj);
    return true;
  }
};

class AutoConnectCommand : public CommandData
{
  AutoConnectDialog dialog;
public:
  //| CommandData Overrides

  Bool Execute(BaseDocument* doc)
  {
    return dialog.Open(DLG_TYPE_ASYNC, ID_AUTOCONNECT_COMMAND);
  }

  Bool RestoreLayout(void* secref)
  {
    return dialog.RestoreLayout(ID_AUTOCONNECT_COMMAND, 0, secref);
  }
};


Bool RegisterAutoConnect()
{
  menu::root().AddPlugin(ID_AUTOCONNECT_COMMAND);
  // RegisterAutoConnectObject();
  return RegisterCommandPlugin(
      ID_AUTOCONNECT_COMMAND,
      GeLoadString(IDS_AUTOCONNECT_NAME),
      PLUGINFLAG_HIDEPLUGINMENU,
      nr::c4d::auto_bitmap("res/icons/autoconnect.tif"),
      GeLoadString(IDS_AUTOCONNECT_HELP),
      NewObjClear(AutoConnectCommand));
}
