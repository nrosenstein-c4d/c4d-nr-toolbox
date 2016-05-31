/**
 * Copyright (c) 2013-2016  Niklas Rosenstein
 * All rights reserved.
 *
 * @file nr/depmanager.h
 * @brief Public API for the Dependency Manager plugin.
 */

#pragma once
#include <c4d.h>
#include <customgui_inexclude.h>

namespace nr {
namespace depmanager {

  /**
   * #PLUGINTYPE for Dependency Manager plugins. Required for
   * #RegisterPlugin().
   */
  static const PLUGINTYPE PLUGINTYPE_DEPENDENCYTYPE = (PLUGINTYPE) 1031033;

  /**
   * Display settings for an element in the Dependency Manager window.
   */
  struct DisplayFlags
  {
    unsigned activatable : 1;  /// true to display the checkmark
    unsigned active : 1;       /// true if the checkmark should green
    unsigned dimmed : 1;       /// true if to dimm the element

    DisplayFlags() : activatable(false), active(false), dimmed(false) { }
  };

  enum
  {
    /**
     * This message is sent for any clicks on an element in the
     * Dependency Manager. The passed data is a pointer to #MsgMouseEvent.
     *
     * Note that this message is always sent, so you should check if
     * you actually want to handle it first (eg. by checking the
     * #DisplayFlags::activatable flag).
     */
    DEPMSG_MOUSEEVENT = 1031044,

    /**
     * Sent for retreiving the icon of the element. The passed data is
     * a pointer to #MsgGetIcon
     */
    DEPMSG_GETICON = 1031041,

    /**
     * Sent for retreiving the reference icon of an element (eg. the
     * dynamics tag icon in the case of the "Dynamics Forces"). The
     * passed data is a #MsgGetIcon pointer.
     */
    DEPMSG_GETREFICON = 1031042,

    /**
     * This message is sent to deselect all elements of your dependency
     * plugin's category. The `parent` and `element` parameters to
     * #DependencyTypData::Message() are guaranteed to be nullptr. The passed
     * data is a pointer to #BaseDocument.
     */
    DEPMSG_DESELECTALL = 1031043,
  };

  /**
   * These are the IDs of the columns in the Dependency Manager's
   * Tree View. Some messages, like the #MsgMouseEvent, might contain
   * the column ID for which the event occured.
   */
  enum DEPCOLUMN {
    DEPCOLUMN_0,
    DEPCOLUMN_ELEMENT,
    DEPCOLUMN_REFICON,
    DEPCOLUMN_CHECKMARK,
    DEPCOLUMN_LAYER,
  };

  /**
   * Sent with #DEPMSG_MOUSEEVENT.
   */
  struct MsgMouseEvent {
    /**
     * Information about the mouse-state. Guaranteed to be not `nullptr`.
     */
    MouseInfo* info;

    /**
     * The column the event occured in.
     */
    DEPCOLUMN column;

    /**
     * Shortcut, can be used instead of calling @meth`BaseList2D::GetDocument()`.
     * Is guaranteed to be not `nullptr`.
     */
    BaseDocument* doc;

    /**
     * The display flags of the item in the Dependency Manager.
     */
    DisplayFlags display_flags;

    /**
     * Shortcut for retreiving this information form the event container
     * in @attr`info`. `true` if the event was invoked with a right mouse
     * button click, otherwise `false`.
     */
    Bool right_button;

    /**
     * True if this is a double-click. As of a limitation of Cinema 4D's
     * tree-view, this can not come in combination with @attr`right_button`.
     */
    Bool double_click;

    /**
     * Set this to `true` if the event was handled. This will prevent
     * the default action.
     */
    Bool handled;

    MsgMouseEvent()
    : info(nullptr), column(DEPCOLUMN_0), doc(nullptr),
      display_flags(), right_button(false), double_click(false),
      handled(false) { }
  };

  /**
   * Message data for #DEPMSG_GETICON and #DEPMSG_GETREFICON.
   */
  struct MsgGetIcon {
    /**
     * Fill this with the icon information.
     */
    IconData icon;

    /**
     * If this is `true`, the icon will be drawn with a colored
     * frame indicating its active state.
     */
    Bool selection_frame;

    /**
     * Set this to `true` to specify that the caller must free
     * the bitmap in @attr`icon.bmp`.
     */
    Bool caller_is_owner;

    MsgGetIcon()
    : selection_frame(false), caller_is_owner(false) { }
  };

  /**
   * Base class for implementing a new Dependency Manager category plugin.
   */
  class DependencyTypeData : public BaseData {
  private:
    typedef BaseData super;
  public:
    DependencyTypeData();

    virtual ~DependencyTypeData();

    /**
     * Override this method to specify the first element of the tree
     * that will be used to search for dependencies.
     */
    virtual BaseList2D* GetFirstElement(BaseDocument* doc) = 0;

    /**
     * Override this method to specify if the specified element is
     * allowed as a dependency for the element.
     */
    virtual Bool AcceptDependency(BaseList2D* element, BaseList2D* pot_dependency);

    /**
     * Returns the default flags for inserting the specified object
     * into the elements dependencies.
     */
    virtual Int32 GetDefaultInExFlags(BaseList2D* element, BaseList2D* pot_dependency);

    /**
     * Override this method to specify the @class`InExcludeData` which
     * will be used as the dependencies of the current element in the
     * tree. Fill the passed `dest` parameter with the list representing
     * the element's dependencies.
     */
    virtual Bool GetElementDependencies(BaseList2D* element, GeData& dest) = 0;

    /**
     * Override this method to change the dependencies of an element. You
     * may validate the passed data by adding or removing elements.
     */
    virtual Bool SetElementDependencies(BaseList2D* element, const GeData& data) = 0;

    /**
     * Override this method to specify display flags for an element in
     * the Dependency Manager tree. If `parent` is `nullptr`, the element
     * is a normal element, if the `parent` is set, the element is a
     * a dependency of `parent`. This method may be called multiple times
     * for the same element.
     *
     * `parent_list` is the @class`InExcludeData` returned from
     * @meth`GetElementDependencies()`. However, it can be `nullptr` in which
     * case you can still use the method mentioned earlier.
     */
    virtual DisplayFlags GetDisplayFlags(BaseList2D* element, BaseList2D* parent, InExcludeData* parent_list) = 0;

    /**
     * Override. This method is called when an element is removed from the Dependency
     * Manager. Return `true` if the element was removed, `false` if not. Make
     * sure to deallocate the removed element and add an undo.
     *
     * The default implementation removes the object from the document if it has
     * no parent (ie. it is a source node) or removes it from the
     * @class`InExcludeData` that is retrieved/set via the methods in this
     * class.
     */
    virtual Bool RemoveElement(BaseList2D* element, BaseList2D* parent);

    /**
     * Override. This method is called for various events.
     */
    virtual Bool Message(BaseList2D* element, BaseList2D* parent, Int32 type, void* p_data);

    /**
     * Override. Called when an elements in the Dependency Manager are about to
     * be selected. The `mode` parameter will be either `SELECTION_NEW`, `SELECTION_ADD`
     * or `SELECTION_SUB`, however you do not have to handle this! Right after
     * this method, @meth`SelectElement` will be called with exactly the same mode.
     *
     * You can return a pointer to custom data that will be passed to the following
     * calls of @meth`SelectElement()` and @meth`SelectionEnd()`.
     *
     * @note: An undo is added automatically _before_ this method is called.
     */
    virtual void* SelectionStart(BaseDocument* doc, Int32 mode);

    /**
     * Override. Called to inform you that the selection state of an object
     * is about to change. If you return `true`, it means the event was handled,
     * otherwise the caller can handle this pretty good, too.
     *
     * @note: The `element` parameter can be `nullptr`. This is usually the case
     * when `mode` equals `SELECTION_NEW`, but not necessarily.
     */
    virtual Bool SelectElement(BaseList2D* element, BaseList2D* parent, Int32 mode, void* userdata);

    /**
     * Override. Called to inform you that the selection-change as stopped. This
     * is usually override to activate another mode in the Attributes Manager.
     *
     * @note: The undo started by the caller before @meth`SelectionStart()`
     * is ended right _after_ this method is called.
     */
    virtual void SelectionEnd(BaseDocument* doc, void* userdata);

  };

  struct DEPENDENCYTYPEPLUGIN : public STATICPLUGIN {
    BaseList2D* (DependencyTypeData::*GetFirstElement)(BaseDocument* doc);
    Bool (DependencyTypeData::*AcceptDependency)(BaseList2D* element, BaseList2D* pot_dependency);
    Int32 (DependencyTypeData::*GetDefaultInExFlags)(BaseList2D* element, BaseList2D* pot_dependency);
    Bool (DependencyTypeData::*GetElementDependencies)(BaseList2D* element, GeData& dest);
    Bool (DependencyTypeData::*SetElementDependencies)(BaseList2D* element, const GeData& dest);
    DisplayFlags (DependencyTypeData::*GetDisplayFlags)(BaseList2D* element, BaseList2D* parent, InExcludeData* parent_list);
    Bool (DependencyTypeData::*RemoveElement)(BaseList2D* element, BaseList2D* parent);
    Bool (DependencyTypeData::*Message)(BaseList2D* element, BaseList2D* parent, Int32 type, void* p_data);
    void* (DependencyTypeData::*SelectionStart)(BaseDocument* doc, Int32 mode);
    Bool (DependencyTypeData::*SelectElement)(BaseList2D* element, BaseList2D* parent, Int32 mode, void* userdata);
    void (DependencyTypeData::*SelectionEnd)(BaseDocument* doc, void* userdata);
  };

  class DependencyType {

    DEPENDENCYTYPEPLUGIN* m_plugin;

  public:

    DependencyType()
    : m_plugin(nullptr) { }

    DependencyType(Int32 pluginid) {
      m_plugin = nullptr;
      BasePlugin* plug = FindPlugin(pluginid, PLUGINTYPE_DEPENDENCYTYPE);
      if (plug) {
        m_plugin = (DEPENDENCYTYPEPLUGIN*) plug->GetPluginStructure();
      }
    }

    DependencyType(DEPENDENCYTYPEPLUGIN* plugin)
    : m_plugin(plugin) { }

    ~DependencyType() { }

    DEPENDENCYTYPEPLUGIN* GetPlugin() const {
      return m_plugin;
    }

    DEPENDENCYTYPEPLUGIN*& GetPlugin() {
      return m_plugin;
  }

    DependencyTypeData* GetData() const {
      if (m_plugin) {
        return static_cast<DependencyTypeData*>(m_plugin->adr);
      }
      return nullptr;
    }

    BaseList2D* GetFirstElement(BaseDocument* doc) {
      DependencyTypeData* data = GetData();
      if (data) return (data->*(m_plugin->GetFirstElement))(doc);
      return nullptr;
    }

    Bool AcceptDependency(BaseList2D* element, BaseList2D* pot_dependency) {
      DependencyTypeData* data = GetData();
      if (data) return (data->*(m_plugin->AcceptDependency))(element, pot_dependency);
      return false;
    }

    Int32 GetDefaultInExFlags(BaseList2D* element, BaseList2D* pot_dependency) {
      DependencyTypeData* data = GetData();
      if (data) return (data->*(m_plugin->GetDefaultInExFlags))(element, pot_dependency);
      return false;
    }

    Bool GetElementDependencies(BaseList2D* element, GeData& dest) {
      DependencyTypeData* data = GetData();
      if (data) return (data->*(m_plugin->GetElementDependencies))(element, dest);
      return false;
    }

    Bool SetElementDependencies(BaseList2D* element, const GeData& gedata) {
      DependencyTypeData* data = GetData();
      if (data) return (data->*(m_plugin->SetElementDependencies))(element, gedata);
      return false;
    }

    DisplayFlags GetDisplayFlags(BaseList2D* element, BaseList2D* parent, InExcludeData* parent_list) {
      DependencyTypeData* data = GetData();
      if (data) return (data->*(m_plugin->GetDisplayFlags))(element, parent, parent_list);
      return {};
    }

    Bool RemoveElement(BaseList2D* element, BaseList2D* parent) {
      DependencyTypeData* data = GetData();
      if (data) return (data->*(m_plugin->RemoveElement))(element, parent);
      return false;
    }

    Bool Message(BaseList2D* element, BaseList2D* parent, Int32 type, void* p_data) {
      DependencyTypeData* data = GetData();
      if (data) return (data->*(m_plugin->Message))(element, parent, type, p_data);
      return false;
    }

    void* SelectionStart(BaseDocument* doc, Int32 mode) {
      DependencyTypeData* data = GetData();
      if (data) return (data->*(m_plugin->SelectionStart))(doc, mode);
      return nullptr;
    }

    Bool SelectElement(BaseList2D* element, BaseList2D* parent, Int32 mode, void* userdata) {
      DependencyTypeData* data = GetData();
      if (data) return (data->*(m_plugin->SelectElement))(element, parent, mode, userdata);
      return false;
    }

    void SelectionEnd(BaseDocument* doc, void* userdata) {
      DependencyTypeData* data = GetData();
      if (data) (data->*(m_plugin->SelectionEnd))(doc, userdata);
    }

    void DeselectAll(BaseDocument* doc) {
      Message(nullptr, nullptr, DEPMSG_DESELECTALL, doc);
    }

  };

  /**
   * Fills the passed @class`AtomArray` with a list of all registered
   * @class`DependencyTypeData` plugins. The list can be sorted before being
   * appended to the array by passing `sort=true`.
   */
  void GetDependencyTypePlugins(AtomArray* arr, Bool sort);

  Bool FillDependencyTypePlugin(DEPENDENCYTYPEPLUGIN* plugin);

  Bool RegisterDependencyType(Int32 pluginid, Int32 info, const String& name, DependencyTypeData* plugin);

  /**
   * Get an #InExcludeData from a #GeData object.
   */
  inline InExcludeData* GetInExcludeData(GeData& data) {
    return (InExcludeData*) data.GetCustomDataType(CUSTOMDATATYPE_INEXCLUDE_LIST);
  }

}} // namespace nr::dependencies
