/**
 * Copyright (c) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * source/DependencyTypeExtensions.cpp
 */

#define PRINT_PREFIX "[nr-toolbox/DependencyManager]: "

#include <c4d.h>
#include <c4d_baseeffectordata.h>
#include <lib_activeobjectmanager.h>
#include <dynrigidbodytag.h>
#include <obasemogen.h>
#include <NiklasRosenstein/c4d/utils.hpp>
#include <NiklasRosenstein/c4d/cleanup.hpp>
#include "res/c4d_symbols.h"
#include "depmanager.h"

namespace nr { using namespace niklasrosenstein; }
using namespace nr::depmanager;

enum
{
  ID_TAG_RIGIDBODY = 180000102,
  EFFECTORS_INEXCLUDELIST_ENABLED = (1 << 0),
};


class BaseObjectDependencyType : public DependencyTypeData {

  typedef DependencyTypeData super;

protected:

  /**
   * The parameter ID to retrive the @class`InExcludeData` from.
   */
  Int32 m_paramid;

  /**
   * A bitfield of modes (specifieng what elements are deselected
   * on `DEPMSG_DESELECTAL`, for instance.
   */
  Int32 m_mode;

  /**
   * A mode specifieng the main mode. Used for example after elements
   * have been selected to activate the respective Attribute Manager
   * type.
   */
  Int32 m_main;

  /**
   * A type-id used based on the @attr`m_main` value.
   *
   * - `Mode_Objects`: Dependency objects must be of this type.
   * - `Mode_Tags`: A tag of this type is retreived from an object.
   * - `Mode_Materials`: Not yet defined..
   */
  Int32 m_type_id;

  /**
   * A list of accepted dependency IDs. If the list is empty, all
   * potential dependencies are accepted.
   */
  maxon::BaseArray<Int32> m_deptypes;

  /**
   * A list of info flags that are accepted as object dependencies.
   */
  struct InfoTuple {
    Int32 base_id;
    Int32 flags;
  };
  maxon::BaseArray<InfoTuple> m_depinfos;

  /**
   * A bitfield which is toggled in the flags of an item in an
   * @class`InExcludeData` for activation (eg. effectors). Can be
   * zero to active/deactivate as default (eg. forces).
   */
  Int32 m_subcheckmark_flag;

protected:

  /**
   * Subclasses may call this method to make the AcceptDependency()
   * method accept the specified type id.
   */
  void AddAcceptedDependency(Int32 type_id) {
    m_deptypes.Append(type_id);
  }

  void AddAcceptedDependency(Int32 base_id, Int32 flags) {
    InfoTuple tuple = {base_id, flags};
    m_depinfos.Append(tuple);
  }

public:

  enum Mode {
    Mode_Objects = (1 << 0),
    Mode_Tags = (1 << 1),
    Mode_Materials = (1 << 2),
  };

  BaseObjectDependencyType(Int32 paramid, Mode main, Int32 subs=0, Int32 type_id=NOTOK)
  : super() {
    m_paramid = paramid;
    m_main = main;
    m_mode = main | subs;
    m_type_id = type_id;
    m_subcheckmark_flag = 0;
  }

  BaseList2D* GetElementFromElement(BaseList2D* element) {
    if (!element) return nullptr;
    if (m_main == Mode_Tags && element->IsInstanceOf(Obase)) {
      if (m_type_id == NOTOK) return nullptr;
      element = static_cast<BaseObject*>(element)->GetTag(m_type_id);
    }

    return element;
  }

  //| DependencyTypeData Overrides

  virtual BaseList2D* GetFirstElement(BaseDocument* doc) {
    if (!doc) return nullptr;
    switch (m_main) {
      case Mode_Objects:
      case Mode_Tags:
        return doc->GetFirstObject();
      case Mode_Materials:
        return doc->GetFirstMaterial();
    }
    return nullptr;
  }

  virtual Bool AcceptDependency(BaseList2D* element, BaseList2D* pot_dependency) {
    if (m_deptypes.GetCount() <= 0 && m_depinfos.GetCount() <= 0)
      return true;

    for (Int32 i=0; i < m_deptypes.GetCount(); i++) {
      if (pot_dependency->IsInstanceOf(m_deptypes[i])) return true;
    }

    for (Int32 i=0; i < m_depinfos.GetCount(); i++) {
      const InfoTuple& info = m_depinfos[i];
      if (pot_dependency->IsInstanceOf(info.base_id)) {
        if (pot_dependency->GetInfo() & info.flags)
          return true;
      }
    }

    return false;
  }

  virtual Bool GetElementDependencies(BaseList2D* element, GeData& dest) {
    element = GetElementFromElement(element);
    if (!element) return false;
    DescID const id(DescLevel(m_paramid, CUSTOMDATATYPE_INEXCLUDE_LIST, 0));

    // Check if there should be such a parameter.
    AutoAlloc<Description> desc;
    if (!desc) return false;
    desc->SetSingleDescriptionMode(id);
    if (!element->GetDescription(desc, DESCFLAGS_DESC_0)) return false;

    // Does the parmaeter actually exist?
    BaseContainer* ibc = desc->GetParameterI(id, nullptr);
    if (!ibc) return false;

    element->GetParameter(m_paramid, dest, DESCFLAGS_GET_0);
    if (dest.GetType() == DA_NIL) {
      dest = GeData(CUSTOMDATATYPE_INEXCLUDE_LIST, DEFAULTVALUE);
    }
    return true;
  }

  virtual Bool SetElementDependencies(BaseList2D* element, const GeData& data) {
    element = GetElementFromElement(element);
    if (!element || data.GetType() != CUSTOMDATATYPE_INEXCLUDE_LIST) return false;
    BaseDocument* doc = element->GetDocument();
    if (doc) doc->AddUndo(UNDOTYPE_CHANGE_SMALL, element);
    return element->SetParameter(m_paramid, data, DESCFLAGS_SET_0);
  }

  virtual DisplayFlags GetDisplayFlags(BaseList2D* element, BaseList2D* parent, InExcludeData* parent_list) {
    if (!element) return {};
    DisplayFlags flags;
    if (element->IsInstanceOf(Obase)) {
      flags.activatable = true;
      if (static_cast<BaseObject*>(element)->GetDeformMode()) {
        flags.active = true;
      }
    }
    return flags;
  }

  virtual Bool Message(BaseList2D* element, BaseList2D* parent, Int32 type, void* p_data) {
    switch (type) {
      case DEPMSG_GETREFICON: {
        MsgGetIcon* data = (MsgGetIcon*) p_data;
        switch (m_main) {
          case Mode_Objects:
            return false;
          case Mode_Tags: {
            BaseTag* tag = static_cast<BaseTag*>(GetElementFromElement(element));
            if (tag && !parent && element->IsInstanceOf(Obase)) {
              if (tag->GetBit(BIT_ACTIVE)) data->selection_frame = true;
              return super::Message(tag, parent, DEPMSG_GETICON, data);
            }
            break;
          }
          case Mode_Materials:
            // TODO
            break;
        }
        return true;
      }
      case DEPMSG_MOUSEEVENT: {
        if (!element || !p_data) break;
        MsgMouseEvent* event = (MsgMouseEvent*) p_data;

        Bool activatable = event->display_flags.activatable;
        Bool clicked_element = event->column == DEPCOLUMN_ELEMENT;
        Bool clicked_checkmark = event->column == DEPCOLUMN_CHECKMARK &&
            activatable && !event->right_button;
        Bool clicked_reficon = event->column == DEPCOLUMN_REFICON &&
            !event->right_button && !event->double_click;

        if (clicked_checkmark && parent && m_subcheckmark_flag != 0) {
          GeData data;
          if (!GetElementDependencies(parent, data)) break;

          InExcludeData* list = nr::c4d::get<InExcludeData>(data);
          if (!list) break;

          Int32 index = list->GetObjectIndex(element->GetDocument(), element);
          if (index < 0) break;

          Int32 flags = list->GetFlags(index);
          Bool state = event->display_flags.active;
          if (state) flags &= ~m_subcheckmark_flag;
          else flags |= m_subcheckmark_flag;
          list->SetFlags(index, flags);

          event->doc->AddUndo(UNDOTYPE_CHANGE_SMALL, parent);
          SetElementDependencies(parent, data);
          event->handled = true;
        }
        else if (clicked_checkmark || (clicked_element && event->double_click)) {
          if (element->IsInstanceOf(Obase)) {
            event->handled = true;
            BaseObject* obj = static_cast<BaseObject*>(element);
            event->doc->AddUndo(UNDOTYPE_CHANGE_SMALL, element);
            obj->SetDeformMode(!obj->GetDeformMode());
          }
        }
        else if (clicked_reficon && m_main == Mode_Tags) {
          BaseTag* tag = static_cast<BaseTag*>(GetElementFromElement(element));
          if (tag) {
            event->handled = true;
            event->doc->SetActiveTag(tag);
            ActiveObjectManager_SetMode(ACTIVEOBJECTMODE_TAG, false);
          }
        }
        break;
      }
      case DEPMSG_DESELECTALL: {
        if (!p_data) break;
        BaseDocument* doc = (BaseDocument*) p_data;

        if (m_mode & Mode_Objects) {
          doc->SetActiveObject(nullptr);
        }
        if (m_mode & Mode_Tags) {
          doc->SetActiveTag(nullptr);
        }
        if (m_mode & Mode_Materials) {
          doc->SetActiveMaterial(nullptr);
        }
        break;
      }
    }

    return super::Message(element, parent, type, p_data);
  }

  virtual Bool ElementDoubleClick(BaseList2D* element, BaseList2D* parent, MouseInfo* mouse) {
    if (element && element->IsInstanceOf(Obase)) {
      BaseDocument* doc = element->GetDocument();
      BaseObject* obj = static_cast<BaseObject*>(element);

      if (doc) doc->AddUndo(UNDOTYPE_CHANGE_SMALL, obj);
      obj->SetDeformMode(!obj->GetDeformMode());
      return true;
    }
    return false;
  }

  virtual void SelectionEnd(BaseDocument* doc, void* userdata) {
    ACTIVEOBJECTMODE mode;
    switch (m_main) {
      case Mode_Objects:
      case Mode_Tags:
        mode = ACTIVEOBJECTMODE_OBJECT;
        break;
      case Mode_Materials:
        mode = ACTIVEOBJECTMODE_MATERIAL;
        break;
      default:
        return;
    }
    ActiveObjectManager_SetMode(mode, false);
  }

};

#define ID_DEPENDENCYTYPE_EFFECTORS 1031034
class EffectorDependencyType : public BaseObjectDependencyType {

  typedef BaseObjectDependencyType super;

public:

  EffectorDependencyType()
  : super(ID_MG_MOTIONGENERATOR_EFFECTORLIST, super::Mode_Objects) {
    m_subcheckmark_flag = EFFECTORS_INEXCLUDELIST_ENABLED;
    AddAcceptedDependency(Obaseeffector);
  }

  //| DependencyTypeData Overrides

  virtual Int32 GetDefaultInExFlags(BaseList2D* element, BaseList2D* pot_dependency) {
    return EFFECTORS_INEXCLUDELIST_ENABLED;
  }

  virtual DisplayFlags GetDisplayFlags(BaseList2D* element, BaseList2D* parent, InExcludeData* parent_list) {
    if (!element) return {};

    DisplayFlags flags;
    flags.activatable = true;
    if (parent && element->IsInstanceOf(Obaseeffector)) {
      GeData temp_list;
      if (!parent_list) {
        if (GetElementDependencies(parent, temp_list)) parent_list = nr::c4d::get<InExcludeData>(temp_list);
      }

      Int32 index = parent_list->GetObjectIndex(element->GetDocument(), element);
      Int32 itemflags = index >= 0 ? parent_list->GetFlags(index) : 0;

      if (itemflags & EFFECTORS_INEXCLUDELIST_ENABLED) {
        flags.active = true;
      }
      if (!static_cast<BaseObject*>(element)->GetDeformMode()) {
        flags.dimmed = true;
      }
    }
    else {
      flags = super::GetDisplayFlags(element, parent, parent_list);
    }
    return flags;
  }

};

#define ID_DEPENDENCYTYPE_FORCES 1031036
class ForcesDependencyType : public BaseObjectDependencyType {

  typedef BaseObjectDependencyType super;

public:

  ForcesDependencyType()
  : super(RIGID_BODY_FORCE_INCLUDE_LIST, super::Mode_Tags, super::Mode_Objects, ID_TAG_RIGIDBODY) {
    AddAcceptedDependency(Obase, OBJECT_PARTICLEMODIFIER);
  }

};


Bool RegisterDependencyTypeExtensions() {
  RegisterDependencyType(
      ID_DEPENDENCYTYPE_EFFECTORS,
      0,
      GeLoadString(IDC_DEPENDENCYTYPE_EFFECTORS),
      NewObjClear(EffectorDependencyType));
  RegisterDependencyType(
      ID_DEPENDENCYTYPE_FORCES,
      0,
      GeLoadString(IDC_DEPENDENCYTYPE_FORCES),
      NewObjClear(ForcesDependencyType));
  return true;
}
