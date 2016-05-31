/**
 * Copyright (c) 2016  Niklas Rosenstein
 * All rights reserved.
 *
 * @file DependencyManager.cpp
 * @brief Implements the Dependency Manager dialog.
 */

#define PRINT_PREFIX "[nr-toolbox/DependencyManager]: "

#include <c4d.h>
#include <customgui_bitmapbutton.h>
#include <customgui_inexclude.h>
#include <lib_iconcollection.h>
#include <Olayer.h>
#include <nr/c4d/util.h>
#include <nr/c4d/gui.h>
#include <nr/c4d/tree_node.h>
#include <nr/c4d/cleanup.h>
#include "res/c4d_symbols.h"
#include "depmanager.h"
#include "misc/print.h"
#include "misc/raii.h"
#include "menu.h"

using namespace nr::depmanager;

static const Int32 DIALOG_COUNT = 4;
static const Int32 PLUGIN_ID = 1031035;

#define USE_DIRECT_LINKS 1

/* Cell Layouting =============================================================================== */

enum ICONMODE {
  ICONMODE_NONE,
  ICONMODE_ICONDATA,
  ICONMODE_COLOR,
};

enum LAYOUTFLAGS_e {
  LAYOUTFLAGS_ICON = (1 << 0),
  LAYOUTFLAGS_TEXT = (1 << 1),
  LAYOUTFLAGS_WIDTH = (1 << 3),
  LAYOUTFLAGS_HEIGHT = (1 << 4),

  LAYOUTFLAGS_ALL = 07777,
};
typedef Int32 LAYOUTFLAGS;

struct Rect {
  Int32 x1, y1, x2, y2;

  Rect()
  : x1(0), y1(0), x2(0), y2(0) { }

  Bool Contains(Int32 x, Int32 y) {
    return x >= x1 && y >= y1 && x < x2 && y < y2;
  }

  Bool Contains(const MouseInfo* mouse) {
    return Contains(mouse->mx, mouse->my);
  }

  void SetAll(Int32 v) {
    x1 = y1 = x2 = y2 = v;
  }

};

struct IconStruct {
  ICONMODE mode;
  IconData data;
  GeData color;
  Bool owned;
  Int32 size;
  Bool active;
  Int32 bmpflags;
  Bool filled;

  Int32 x;
  Int32 y;

  Rect render_rect;

  IconStruct()
  : mode(ICONMODE_NONE), owned(false), size(16), active(false), bmpflags(BMP_ALLOWALPHA),
    filled(false), x(0), y(0) { }

  Bool IsFilled() const {
    if (mode == ICONMODE_ICONDATA) {
      return filled || data.bmp;
    }
    else if (mode == ICONMODE_COLOR) {
      return true;
    }
    return false;
  }

  void Free() {
    if (owned && data.bmp) {
      filled = true;
      BaseBitmap::Free(data.bmp);
    }
  }

  void ChooseMode() {
    if (data.bmp) mode = ICONMODE_ICONDATA;
    else if (color.GetType() != DA_NIL) mode = ICONMODE_COLOR;
    else mode = ICONMODE_NONE;
  }

  void FromDeptype(DependencyType* deptype, BaseList2D* element,
        BaseList2D* parent, Int32 msg) {

    MsgGetIcon msg_data;
    deptype->Message(element, parent, msg, &msg_data);

    data = msg_data.icon;
    active = msg_data.selection_frame;
    owned = msg_data.caller_is_owner;
    ChooseMode();
  }

  Bool Draw(GeUserArea* area, Int32 xpos, Int32 ypos) {
    Rect& r = render_rect;
    r.x1 = xpos + x;
    r.y1 = ypos + y;
    r.x2 = r.x1 + size - 1;
    r.y2 = r.y1 + size - 1;
    Bool drawn = true;
    switch (mode) {
      case ICONMODE_COLOR:
        area->DrawSetPen(color);
        area->DrawRectangle(r.x1, r.y1, r.x2, r.y2);
        break;
      case ICONMODE_ICONDATA:
        if (!data.bmp) break;
        nr::c4d::draw_icon(area, data, r.x1, r.y1, size, size, bmpflags);
        break;
      default:
        drawn = false;
        r.SetAll(-1);
        break;
    }

    if (active && drawn) {
      area->DrawSetPen(COLOR_TEXT_SELECTED);
      nr::c4d::draw_border(area, r.x1, r.y1, r.x2, r.y2, 1);
    }
    return false;
  }

};

struct CellLayout {

  IconStruct icon;

  String text;
  Int32 textwidth;
  Int32 textx;
  Int32 width;
  Int32 height;
  GeData textcol;

  Rect render_rect;

  CellLayout()
  : textwidth(0), textx(0), width(-1), height(-1) { }

  ~CellLayout() {
    icon.Free();
  }

};

/* DepNode Management =========================================================================== */

class DepNode : public nr::c4d::tree_node_crtp<DepNode> {
  typedef nr::c4d::tree_node_crtp<DepNode> super;
  friend class nr::c4d::tree_node_crtp<DepNode>;
public:

  enum Type {
    Type_Root,
    Type_Source,
    Type_Dependency,
    Type_Any,
  };

  enum Bit {
    Bit_Reused = (1 << 0),
    Bit_Selected = (1 << 1),
    Bit_Closed = (1 << 2),
  };

  CellLayout cell_item;
  CellLayout cell_reficon;
  CellLayout cell_enabled;
  CellLayout cell_layer;
  Int32 list_flags;

  DisplayFlags GetDisplayFlags() const {
    return m_display;
  }

  void SetDisplayFlags(DisplayFlags flags) {
    m_display = flags;
  }

  BaseList2D* GetUpLink(BaseDocument* doc) {
    DepNode* parent = GetUp();
    if (parent) return parent->GetLink(doc);
    return nullptr;
  }

  Int32 GetSelectionMode(BaseDocument* doc) {
    BaseList2D* element = GetLink(doc);
    if (!element) return MODE_OFF;

    Bool active = element->GetBit(BIT_ACTIVE);
    if (CheckType(Type_Source)) {
      return active ? MODE_ON : MODE_OFF;
    }
    else if (CheckType(Type_Dependency)) {
      if (!active) {
        // If the element is not selected, we also want
        // to make sure that it will not be displayed as selected
        // in the dependency manager once the BaseList2D element
        // is re-selected.
        DelBit(Bit_Selected);
        return MODE_OFF;
      }
      if (GetBit(Bit_Selected)) return MODE_ON;
      else return MODE_UNDEF;
    }

    return MODE_OFF;
  }

  void Select(BaseDocument* doc, DependencyType* deptype, Bool state) {
    BaseList2D* element = GetLink(doc);
    if (!element) return;

    if (element->GetDocument() != doc) {
      print::error("Element document does not equal specified document.");
      return;
    }

    if (!element->GetBit(BIT_ACTIVE)) {
      if (doc) doc->AddUndo(UNDOTYPE_BITS, element);
      if (state) element->SetBit(BIT_ACTIVE);
      else element->DelBit(BIT_ACTIVE);
    }
    if (CheckType(Type_Dependency) && deptype) {
      SetBit(Bit_Selected);
    }
  }

  Bool IsOpened(BaseDocument* doc) {
    return !GetBit(Bit_Closed);
  }

  void Open(BaseDocument* doc, Bool state) {
    SetBit(Bit_Closed, !state);
  }

  String GetName(BaseDocument* doc) {
    BaseList2D* element = GetLink(doc);
    if (element) return element->GetName();
    else return "<nullptr>";
  }

  BaseList2D* GetLink(BaseDocument* doc) {
    #if USE_DIRECT_LINKS
      return m_element;
    #else
      return m_link->GetLink(doc);
    #endif
  }

  void SetLink(BaseList2D* element) {
    #if USE_DIRECT_LINKS
      m_element = element;
    #else
      m_link->SetLink(element);
    #endif
  }

  void BitTree(Bit bit, Bool mode) {
    this->iter<DepNode>([bit, mode](DepNode* node) {
      node->set_bit(bit, mode);
      return true;
    });
  }

  static DepNode* Alloc(Type type) {
    DepNode* node = super::Alloc();
    if (node)
      node->m_type = type;
    return node;
  }

  //| nr::node::BaseTreeNode Overrides

  virtual uint32_t type() const { return m_type; }
  virtual bool check_type(uint32_t type) const { return type == m_type || type == Type_Any; }

private:

  DepNode() : super() {
    list_flags = 0;
    #if USE_DIRECT_LINKS
      m_element = nullptr;
    #endif
    m_type = Type_Root;
  }

  virtual ~DepNode() { }

  #if USE_DIRECT_LINKS
    BaseList2D* m_element;
  #else
    AutoAlloc<BaseLink> m_link;
  #endif
  Type m_type;
  DisplayFlags m_display;

  nr::datastructures::tree_node* shallow_copy() const override { return new DepNode; }

};

class DepNodeRebuildTable : private maxon::HashMap<BaseList2D*, DepNode*> {

  typedef maxon::HashMap<BaseList2D*, DepNode*> super;

  DepNode::Type m_type;

public:

  DepNodeRebuildTable(DepNode::Type type=DepNode::Type_Source)
  : m_type(type) { }

  using super::Begin;
  using super::End;

  void BuildTable(DepNode* root, BaseDocument* doc) {
    if (!root) return;

    while (root) {
      if (root->CheckType(m_type)) {
        super::Put(root->GetLink(doc), root);
      }
      root = nr::c4d::get_next_node(root);
    }
  }

  DepNode* FindNode(BaseList2D* element) const {
    const super::Entry* e = super::FindEntry(element);
    if (e) return e->GetValue();
    else return nullptr;
  }

};

class DepNodeManager {

  DepNode* m_root;
  DependencyType m_deptype;
  BaseDocument* m_doc;
  Bool m_showempty;

public:

  DepNodeManager()
  : m_root(nullptr), m_deptype(), m_doc(nullptr), m_showempty(true) { }

  Bool GetShowEmpty() const { return m_showempty; }

  void SetShowEmpty(Bool value) { m_showempty = value; }

  DependencyType* GetDependencyType() { return &m_deptype; }

  BaseDocument* GetDocument() const {
    return m_doc;
  }

  Bool UpdateNodes(const DependencyType& deptype, BaseDocument* doc=nullptr) {
    // Default to the active document.
    if (!doc) doc = GetActiveDocument();

    // Override cached values and stop if there is no document
    // to work on.
    m_doc = doc;
    m_deptype = deptype;
    if (!doc) return false;

    // Retrieve the root node and quit if there is none.
    DepNode* root = GetRoot();
    if (!root) return false;

    // If the DependencyType is un-initialized, we will just flush the whole
    // tree.
    if (deptype.GetPlugin() == nullptr) {
      root->flush();
      return true;
    }

    // Create a rebuild-table which will tell us what nodes do already
    // exist and need not to be re-created (resuling in loss of qualifiers
    // such as opened or selection state).
    DepNodeRebuildTable table;
    table.BuildTable(root, m_doc);

    // Mark the complete tree as being unused, except the root.
    root->BitTree(DepNode::Bit_Reused, false);
    root->SetBit(DepNode::Bit_Reused);

    // Iterate over the elements yielded by the DependencyType.
    BaseList2D* current = m_deptype.GetFirstElement(doc);
    DepNode* prev_node = nullptr;
    while (current) {
      // Retrieve the InExcludeList with the dependencies of the current
      // element. If it doesn't yield a valid list, the object does not
      // need to be represented in the tree.
      GeData ge_list;
      InExcludeData* list = nullptr;
      if (m_deptype.GetElementDependencies(current, ge_list) && ge_list.GetType() == CUSTOMDATATYPE_INEXCLUDE_LIST) {
        list = nr::c4d::get<InExcludeData>(ge_list);
      }

      // Do not show this object in the Dependency Manager if it does not have
      // any dependencies.
      if (!m_showempty && list && list->GetObjectCount() <= 0) {
        list = nullptr;
      }

      if (list) {
        // Is there already a node for the current element?
        DepNode* node = _FindOrCreateNode(current, nullptr, list, table, DepNode::Type_Source);
        if (node) {
          node->InsertAt(root, prev_node);
          prev_node = node;
        }
      }

      current = nr::c4d::get_next_node(current);
    }

    // Remove un-used nodes.
    DepNode* dep_curr = root->GetDown();
    while (dep_curr) {
      DepNode* next;
      if (!dep_curr->GetBit(DepNode::Bit_Reused)) {
        next = nr::c4d::get_next_node_del(dep_curr);
        dep_curr->Remove();
        DepNode::Free(dep_curr);
      }
      else {
        dep_curr->DelBit(DepNode::Bit_Reused);
        next = nr::c4d::get_next_node(dep_curr);
      }

      dep_curr = next;
    }

    return true;
  }

  DepNode* GetRoot() {
    if (!m_root)
      m_root = DepNode::Alloc(DepNode::Type_Root);
    return m_root;
  }

  DepNode* GetFirst() const {
    if (m_root)
      return m_root->GetDown();
    return nullptr;
  }

private:

  DepNode* _FindOrCreateNode(BaseList2D* element, BaseList2D* parent, InExcludeData* list,
        const DepNodeRebuildTable& table, DepNode::Type type) {
    DepNode* node = table.FindNode(element);
    if (!node) {
      node = DepNode::Alloc(type);
      if (!node) {
        print::error("DepNode could not be allocated.");
        return nullptr;
      }
      node->SetLink(element);
    }
    else node->Remove();

    node->SetBit(DepNode::Bit_Reused);
    node->SetDisplayFlags(m_deptype.GetDisplayFlags(element, parent, list));

    if (list && !parent) _CreateDependencyNodes(node, element, list);
    return node;
  }

  void _CreateDependencyNodes(DepNode* source, BaseList2D* element, InExcludeData* list) {
    if (!source || !element || !list) return;

    // Build a table for re-using already available nodes.
    DepNodeRebuildTable table(DepNode::Type_Dependency);
    table.BuildTable(source, m_doc);

    Int32 count = list->GetObjectCount();
    DepNode* prev_node = nullptr;
    for (Int32 i=0; i < count; i++) {
      BaseList2D* current = list->ObjectFromIndex(m_doc, i);
      if (!current) continue;

      DepNode* node = _FindOrCreateNode(current, element, list, table, DepNode::Type_Dependency);
      if (node) {
        // Write the flags of the node in the list to the node so it
        // can be re-used when re-writing the list.
        node->list_flags = list->GetFlags(i);
        node->InsertAt(source, prev_node);
        prev_node = node;
      }
    }
  }

};

/* DependencyManager_Model ====================================================================== */

CellLayout* GetCellLayout(DepNode* node, Int32 column) {
  if (!node) return nullptr;
  switch (column) {
    case DEPCOLUMN_ELEMENT:
      return &node->cell_item;
    case DEPCOLUMN_REFICON:
      return &node->cell_reficon;
    case DEPCOLUMN_CHECKMARK:
      return &node->cell_enabled;
    case DEPCOLUMN_LAYER:
      return &node->cell_layer;
  }
  return nullptr;
}

class DependencyManager_Model : public TreeViewFunctions {

  typedef TreeViewFunctions super;

  Int32 m_hpadding;
  Int32 m_vpadding;
  Bool m_selectionundo_started;
  void* m_selection_depdata;

  Bool _ComputeLayout(DepNode* node, BaseDocument* doc, GeUserArea* area, DependencyType* deptype,
          Int32 column, LAYOUTFLAGS flags, CellLayout* layout) {
    if (!node || !layout || !area || !deptype) return false;

    BaseList2D* link = node->GetLink(doc);
    BaseList2D* parent_link = node->GetUpLink(doc);
    if (!link) return false;

    // If the iconflag is not given, but the size needs to be calculated,
    // this parameter holds true when the column actually does have an icon.
    Bool has_icon = false;

    // If we need to calculate the width, we definitely need the text of the
    // cell. We do also need the icons for proper calculation.
    if (flags & LAYOUTFLAGS_WIDTH) {
      flags |= LAYOUTFLAGS_TEXT;
      flags |= LAYOUTFLAGS_ICON;
    }

    // Reset the layout to make sure there is no leftover
    // data from previous calculations.
    new (layout) CellLayout();

    // Compute text & icons.
    switch (column) {
      case DEPCOLUMN_ELEMENT:
        layout->icon.size = 16;
        if (flags & LAYOUTFLAGS_TEXT) layout->text = node->GetName(doc);
        if (flags & LAYOUTFLAGS_ICON) {
          layout->icon.FromDeptype(deptype, link, parent_link, DEPMSG_GETICON);
        }
        has_icon = true;
        break;
      case DEPCOLUMN_REFICON:
        layout->icon.size = 16;
        if (flags & LAYOUTFLAGS_ICON) {
          layout->icon.FromDeptype(deptype, link, parent_link, DEPMSG_GETREFICON);
        }
        has_icon = true;
        break;
      case DEPCOLUMN_CHECKMARK:
        layout->icon.size = 12;
        if (flags & LAYOUTFLAGS_ICON) {
          DisplayFlags display = node->GetDisplayFlags();
          if (display.activatable) {
            layout->icon.mode = ICONMODE_ICONDATA;
            Int32 icon_id;
            if (display.active) {
              icon_id = RESOURCEIMAGE_OBJECTMANAGER_STATE2; // green checkmark
            }
            else {
              icon_id = RESOURCEIMAGE_OBJECTMANAGER_STATE1; // red cross
            }
            if (!GetIcon(icon_id, &layout->icon.data)) {
              layout->icon.mode = ICONMODE_NONE;
            }
            layout->icon.owned = false;
          }
          has_icon = true;
        }
        break;
      case DEPCOLUMN_LAYER:
        layout->icon.size = 12;
        if (flags & LAYOUTFLAGS_ICON) {
          LayerObject* layer = link->GetLayerObject(doc);
          if (layer && layer->GetParameter(ID_LAYER_COLOR, layout->icon.color, DESCFLAGS_GET_0)) {
            layout->icon.mode = ICONMODE_COLOR;
          }
        }
        has_icon = true;
        break;
      default:
        return false;
    }

    DisplayFlags display = node->GetDisplayFlags();
    if (flags & LAYOUTFLAGS_ICON) {
      if (display.dimmed) {
        layout->icon.bmpflags |= BMP_DIMIMAGE;
      }
    }
    if (flags & LAYOUTFLAGS_TEXT) {
      Int32 selmode = node->GetSelectionMode(doc);
      switch (selmode) {
        case MODE_UNDEF:
          layout->textcol = COLOR_TEXT_SELECTED_DARK;
          break;
        case MODE_ON:
          layout->textcol = COLOR_TEXT_SELECTED;
          break;
        case MODE_OFF:
        default:
          if (display.dimmed) layout->textcol = COLOR_TEXT_DISABLED;
          else layout->textcol = COLOR_TEXT;
          break;
      }
    }
    if (flags & LAYOUTFLAGS_WIDTH) {
      layout->textx = 0;
      if (layout->icon.mode != ICONMODE_NONE) layout->textx  = layout->icon.size + m_hpadding;
      layout->textwidth = area->DrawGetTextWidth(layout->text);

      layout->width = layout->textwidth + layout->textx;
      layout->icon.x = 0;
    }
    if (flags & LAYOUTFLAGS_HEIGHT) {
      Int32 text_height = area->DrawGetFontHeight();
      layout->height = maxon::Max(text_height, layout->icon.size);

      layout->icon.y = 0; // Computed in DrawCell()
    }

    return true;
  }

  Bool _ComputeLayout(void* root, void* ud, void* obj, Int32 column, GeUserArea* area,
          LAYOUTFLAGS flags, CellLayout* layout) {
    if (!root || !ud || !obj || !area) return false;

    DepNodeManager* manager = (DepNodeManager*) root;
    DependencyType* deptype = manager->GetDependencyType();
    BaseDocument* doc = static_cast<BaseDocument*>(ud);
    DepNode* node = (DepNode*) obj;
    if (!deptype) return false;

    return _ComputeLayout(node, doc, area, deptype, column, flags, layout);
  }

public:

  DependencyManager_Model()
  : TreeViewFunctions(), m_hpadding(3), m_vpadding(2), m_selectionundo_started(false) { }

  void SetTreeLayout(TreeViewCustomGui* treeview) {
    if (!treeview) return;
    BaseContainer layout;
    layout.SetInt32(DEPCOLUMN_ELEMENT, LV_USERTREE);
    layout.SetInt32(DEPCOLUMN_REFICON, LV_USER);
    layout.SetInt32(DEPCOLUMN_CHECKMARK, LV_USER);
    layout.SetInt32(DEPCOLUMN_LAYER, LV_USER);
    treeview->SetLayout(4, layout);
  }

  //| TreeViewFunctions Overrides

  virtual void* GetFirst(void* root, void* ud) {
    if (root) return ((DepNodeManager*) root)->GetFirst();
    else return nullptr;
  }

  virtual void* GetNext(void* root, void* ud, void* obj) {
    if (obj) return ((DepNode*) obj)->GetNext();
    else return nullptr;
  }

  virtual void* GetPred(void* root, void* ud, void* obj) {
    if (obj) return ((DepNode*) obj)->GetPred();
    else return nullptr;
  }

  virtual void* GetDown(void* root, void* ud, void* obj) {
    if (obj) return ((DepNode*) obj)->GetDown();
    else return nullptr;
  }

  virtual void* GetUp(void* root, void* ud, void* obj) {
    if (obj) return ((DepNode*) obj)->GetUp();
    else return nullptr;
  }

  virtual Bool IsSelected(void* root, void* ud, void* obj) {
    if (obj) {
      Int32 mode = ((DepNode*) obj)->GetSelectionMode(static_cast<BaseDocument*>(ud));
      return mode == MODE_ON;
    }
    else return false;
  }

  virtual Bool IsOpened(void* root, void* ud, void* obj) {
    if (obj) return ((DepNode*) obj)->IsOpened(static_cast<BaseDocument*>(ud));
    else return false;
  }

  virtual void Select(void* root, void* ud, void* obj, Int32 mode) {
    if (!root || !ud || !obj) return;
    BaseDocument* doc = static_cast<BaseDocument*>(ud);
    DepNodeManager* manager = (DepNodeManager*) root;
    DependencyType* deptype = manager->GetDependencyType();
    if (!deptype) return;

    // Start a new undo step when this has not been done yet.
    if (!m_selectionundo_started) {
      doc->StartUndo();
      m_selection_depdata = deptype->SelectionStart(doc, mode);
      m_selectionundo_started = true;
    }

    // Will the DependencyType handle the selection event? If not,
    // then do let's do it on our own.
    DepNode* node = (DepNode*) obj;
    DepNode* root_node = manager->GetRoot();
    if (!deptype->SelectElement(node->GetLink(doc), node->GetUpLink(doc), mode, m_selection_depdata)) {
      Bool select_node;
      switch (mode) {
        case SELECTION_NEW:
          if (deptype) deptype->DeselectAll(doc);
          if (root_node) root_node->BitTree(DepNode::Bit_Selected, false);
        case SELECTION_ADD:
          select_node = true;
          break;
        case SELECTION_SUB:
          select_node = false;
          break;
        default:
          doc->EndUndo();
          doc->DoUndo();
          return;
      }

      node->Select(doc, ((DepNodeManager*) root)->GetDependencyType(), select_node);
    }
  }

  virtual void SelectionChanged(void* root, void* ud) {
    if (!root || !ud) return;

    BaseDocument* doc = static_cast<BaseDocument*>(ud);
    DependencyType* deptype = ((DepNodeManager*) root)->GetDependencyType();
    if (m_selectionundo_started && doc && deptype) {
      deptype->SelectionEnd(doc, m_selection_depdata);
      m_selection_depdata = nullptr;
      doc->EndUndo();
      m_selectionundo_started = false;
    }
    EventAdd();
  }

  virtual void Open(void* root, void* ud, void* obj, Bool state) {
    if (obj) return ((DepNode*) obj)->Open(static_cast<BaseDocument*>(ud), state);
    else return;
  }

  virtual String GetName(void* root, void* ud, void* obj) {
    if (obj) return ((DepNode*) obj)->GetName(static_cast<BaseDocument*>(ud));
    else return "???";
  }

  virtual Int GetId(void* root, void* ud, void* obj) {
    return (Int) obj;
  }

  virtual void DrawCell(void* root, void* ud, void* obj, Int32 column, DrawInfo* draw, const GeData& bg_color) {
    GeUserArea* area = draw->frame;
    BaseDocument* doc = static_cast<BaseDocument*>(ud);
    DepNode* node = (DepNode*) obj;
    CellLayout* layout = GetCellLayout(node, column);
    if (!doc || !node || !area || !layout) return;

    Int32 x1 = draw->xpos;
    Int32 y1 = draw->ypos;
    Int32 cw = draw->width;
    Int32 ch = draw->height;
    Int32 x2 = x1 + cw - 1;
    Int32 y2 = y1 + ch - 1;
    // Int32 width = cw - m_hpadding * 2;
    Int32 height = ch - m_vpadding * 2;

    // Draw the background color.
    area->DrawSetPen(bg_color);
    area->DrawRectangle(x1, y1, x2, y2);

    const Int32 xpos = x1 + m_hpadding;
    const Int32 ypos = y1 + m_vpadding;
    const Int32 ymid = (y1 + y2) / 2;

    // Perform the missing calculations for the layout.
    layout->icon.y = (height - layout->icon.size) / 2;

    // Render the layout.
    layout->icon.Draw(area, xpos, ypos);
    if (layout->text.Content()) {
      Int32 flags = DRAWTEXT_HALIGN_LEFT | DRAWTEXT_VALIGN_CENTER;
      area->DrawSetTextCol(layout->textcol, bg_color);
      area->DrawText(layout->text, xpos + layout->textx, ymid, flags);
    }

    Rect& r = layout->render_rect;
    r.x1 = x1;
    r.y1 = y1;
    r.x2 = x1 + cw;
    r.y2 = y1 + ch;
  }

  virtual Int32 GetColumnWidth(void* root, void* ud, void* obj, Int32 column, GeUserArea* area) {
    CellLayout* layout = GetCellLayout((DepNode*) obj, column);
    if (!layout) return 0;
    if (!_ComputeLayout(root, ud, obj, column, area, LAYOUTFLAGS_ALL, layout)) return 0;

    return layout->width + m_hpadding * 2;
  }

  virtual Int32 GetLineHeight(void* root, void* ud, void* obj, Int32 column, GeUserArea* area) {
    CellLayout* layout = GetCellLayout((DepNode*) obj, column);
    if (!layout) return 0;
    if (!_ComputeLayout(root, ud, obj, column, area, LAYOUTFLAGS_ALL, layout)) return 0;

    return layout->height + m_vpadding * 2;
  }

  virtual void CreateContextMenu(void* root, void* ud, void* obj, Int32 column, BaseContainer* bc) {
    if (!bc) return;

    switch (column) {
      case DEPCOLUMN_ELEMENT:
        bc->RemoveData(ID_TREEVIEW_CONTEXT_RESET);
        break;
      default:
        bc->FlushAll();
        break;
    }
  }

  virtual Bool DoubleClick(void* root, void* ud, void* obj, Int32 column, MouseInfo* mouse) {
    if (!root || !ud || !obj) return true; // event catched

    Bool handled = false;
    BaseDocument* doc = static_cast<BaseDocument*>(ud);
    DepNode* node = (DepNode*) obj;
    DepNodeManager* manager = (DepNodeManager*) root;
    DependencyType* deptype = manager->GetDependencyType();
    if (deptype) {
      MsgMouseEvent event;
      event.info = mouse;
      event.column = static_cast<DEPCOLUMN>(column);
      event.doc = doc;
      event.display_flags = node->GetDisplayFlags();
      event.double_click = true;

      deptype->Message(node->GetLink(doc), node->GetUpLink(doc), DEPMSG_MOUSEEVENT, &event);
      handled = event.handled;

    }

    // We do not take into account whether the event was handled
    // by the deptype for any column but DEPCOLUMN_ELEMENT because
    // returning true will allow the user to re-name the element.
    if (column == DEPCOLUMN_ELEMENT) {
      return handled;
    }
    else {
      return true;
    }
  }

  virtual Bool MouseDown(void* root, void* ud, void* obj, Int32 column, MouseInfo* mouse, Bool right_button) {
    if (!root || !ud) return false;

    Bool handled = false;
    BaseDocument* doc = static_cast<BaseDocument*>(ud);
    DepNode* node = (DepNode*) obj;
    DepNodeManager* manager = (DepNodeManager*) root;
    DependencyType* deptype = manager->GetDependencyType();

    if (!obj && deptype) {
      deptype->DeselectAll(doc);
      handled = true;
    }
    else if (deptype) {
      MsgMouseEvent event;
      event.info = mouse;
      event.column = static_cast<DEPCOLUMN>(column);
      event.doc = doc;
      event.display_flags = node->GetDisplayFlags();
      event.right_button = right_button;

      deptype->Message(node->GetLink(doc), node->GetUpLink(doc), DEPMSG_MOUSEEVENT, &event);
      handled = event.handled;
    }

    if (!handled) {
      handled = super::MouseDown(root, ud, obj, column, mouse, right_button);
    }
    return handled;
  }

  virtual void DeletePressed(void* root, void* ud) {
    if (!root || !ud) return;

    BaseDocument* doc = static_cast<BaseDocument*>(ud);
    DepNodeManager* manager = (DepNodeManager*) root;
    DependencyType* deptype = manager->GetDependencyType();
    DepNode* node = manager->GetFirst();
    if (!deptype || !node) return;

    doc->StartUndo();
    while (node) {
      DepNode* next = nr::c4d::get_next_node(node);
      if (node->GetSelectionMode(doc) == MODE_ON) {
        if (deptype->RemoveElement(node->GetLink(doc), node->GetUpLink(doc))) {
          next = nr::c4d::get_next_node(node);
          node->Remove();
          DepNode::Free(node);
        }
      }

      node = next;
    }
    doc->EndUndo();
    EventAdd();
  }

  virtual Int32 GetDragType(void* root, void* ud, void* obj) {
    DepNode* node = static_cast<DepNode*>(obj);
    if (node && node->GetType() == DepNode::Type_Dependency) {
      return DRAGTYPE_ATOMARRAY;
    }
    return 0;
  }

  virtual void GenerateDragArray(void* root, void* ud, void* obj, AtomArray* arr) {
    if (!root || !ud || !arr)
      return;

    BaseDocument* doc = static_cast<BaseDocument*>(ud);
    FillAtomArray(static_cast<DepNodeManager*>(root)->GetRoot(), doc, arr);
  }

  void FillAtomArray(DepNode* current, BaseDocument* doc, AtomArray* arr, Int32 depth=0) {
    if (current->GetSelectionMode(doc) == MODE_ON) {
      BaseList2D* element = current->GetLink(doc);
      if (element) arr->Append(element);
    }

    DepNode* child = current->GetDown();
    while (child) {
      FillAtomArray(child, doc, arr, depth + 1);
      child = child->GetNext();
    }
  }

  struct AtomDrag {
    DepNodeManager* manager;
    DependencyType* type;
    BaseDocument* doc;
    DepNode* source;
    BaseList2D* source_link;
    DepNode* dest;
    AtomArray* array;
    Int32 insertmode;
  };

  Bool AcceptAtomDrag(void* root, void* ud, void* obj, void* object, AtomDrag* data) {
    CriticalAssert(data != nullptr);

    // Fill the data.
    data->manager = static_cast<DepNodeManager*>(root);
    data->type = (data->manager != nullptr ? data->manager->GetDependencyType() : nullptr);
    data->doc = static_cast<BaseDocument*>(ud);
    data->dest = static_cast<DepNode*>(obj);
    data->source = data->dest;
    while (data->source && !data->source->CheckType(DepNode::Type_Source)) {
      data->source = data->source->GetUp();
    }
    data->source_link = (data->source != nullptr ? data->source->GetLink(data->doc) : nullptr);
    data->array = static_cast<AtomArray*>(object);

    // Validate pointers.
    if (!data->manager || !data->type || !data->doc || !data->dest
          || !data->source || !data->source_link || !data->array)
      return false;

    // If there is no element in the array, we don't accept the drag.
    if (data->array->GetCount() <= 0)
      return false;

    // Validate data.
    // Retrieve the object that will recieve the dependencies.
    BaseList2D* element = data->source->GetLink(data->doc);
    if (element == nullptr)
      return false;

    // They must all be acceptable.
    for (Int32 i=0; i < data->array->GetCount(); i++) {
      BaseList2D* sub = static_cast<BaseList2D*>(data->array->GetIndex(i));
      if (sub && !data->type->AcceptDependency(element, sub))
        return false;
    }

    // INSERT_UNDER if the destination node is a source object,
    // INSERT_AFTER and INSERT_BEFORE if it is a dependency object.
    if (data->dest->CheckType(DepNode::Type_Source))
      data->insertmode = INSERT_UNDER;
    else
      data->insertmode = INSERT_AFTER | INSERT_BEFORE;

    return true;
  }

  virtual Int32 AcceptDragObject(void* root, void* ud, void* obj, Int32 type, void* object, Bool& allow_copy) {
    if (type == DRAGTYPE_ATOMARRAY) {
      AtomDrag data;
      if (!AcceptAtomDrag(root, ud, obj, object, &data)) {
        return 0;
      }

      return data.insertmode;
    }
    return 0;
  }

  virtual void InsertObject(void* root, void* ud, void* obj, Int32 type, void* object, Int32 mode, Bool copy) {
    if (type == DRAGTYPE_ATOMARRAY) {
      AtomDrag data;
      if (!AcceptAtomDrag(root, ud, obj, object, &data)) {
        print::error("InsertObject(), AcceptAtomDrag() returned false");
        return;
      }

      if (!(data.insertmode & mode)) {
        print::error("InsertObject(), insert `mode` does not match AtomDrag::insertmode");
        return;
      }

      // Get the destination include lists.
      GeData d_dest;
      data.type->GetElementDependencies(data.source_link, d_dest);
      InExcludeData* l_dest = nr::c4d::get<InExcludeData>(d_dest);
      if (l_dest == nullptr)
        return;

      // This list will contain all objects that have been popped
      // from the in exclude list starting with the object at which
      // the new ones will be inserted.
      struct Item {
        BaseList2D* op;
        Int32 flags;
      };
      maxon::BaseArray<Item> reinsert;

      // Determine starting with which object the objects are removed
      // from the inexclude list.
      BaseList2D* start_node = data.dest->GetLink(data.doc);
      if (mode == INSERT_UNDER) {
        start_node = l_dest->ObjectFromIndex(data.doc, 0);
      }

      // Iterate over the inexclude list and remove all objects
      // after (and maybe including) `start_node`.
      Bool rem = false;
      for (Int32 i=0; i < l_dest->GetObjectCount() && start_node; i++) {
        BaseList2D* current = l_dest->ObjectFromIndex(data.doc, i);
        if (current == nullptr)
          continue;

        // If the insert mode is "BEFORE" or "UNDER", we will include
        // the current object for removing.
        if (!rem && mode != INSERT_AFTER && current == start_node)
          rem = true;

        // Remove the item from the inexclude list and put it
        // on the re-insert list.
        if (rem) {
          Item item = {current, l_dest->GetFlags(i)};
          reinsert.Append(item);
          l_dest->DeleteObject(data.doc, current);
          i--;
        }

        // If the current object is equal to the start object,
        // from this point on all other objects should be removed.
        if (!rem && current == start_node)
          rem = true;
      }

      // Insert the dragged nodes into the destination list.
      for (Int32 i=0; i < data.array->GetCount(); i++) {
        BaseList2D* current = (BaseList2D*) data.array->GetIndex(i);
        if (current == nullptr)
          continue;

        // If the node exists already in the list, delete it (and
        // reuse its flags).
        Int32 oindex = l_dest->GetObjectIndex(data.doc, current);
        Int32 flags = data.type->GetDefaultInExFlags(data.source_link, current);
        if (oindex != NOTOK) {
          flags = l_dest->GetFlags(oindex);
          l_dest->DeleteObject(data.doc, current);
        }

        l_dest->InsertObject(current, flags);
      }

      // Re-insert the removed items to remain the correct order, but
      // only if they are not already present in the list.
      for (Int32 i=0; i < reinsert.GetCount(); i++) {
        const Item& item = reinsert[i];
        if (l_dest->GetObjectIndex(data.doc, item.op) == NOTOK) {
          l_dest->InsertObject(item.op, item.flags);
        }
      }

      data.type->SetElementDependencies(data.source_link, d_dest);
    }
  }

};

/* DependencyManager_Dialog ===================================================================== */

class DependencyManager_Dialog;
static DependencyManager_Dialog* g_dialogs = nullptr;
static Bool OpenDialog(Bool next_closed=false);

class DependencyTypeManager {

  BaseContainer m_types;

public:

  DependencyTypeManager() { }

  void Regather(Bool force=false) {
    if (m_types.GetIndexId(0) != NOTOK && !force)
      return;

    m_types.FlushAll();
    AutoAlloc<AtomArray> plugins;
    if (!plugins)
      return;
    GetDependencyTypePlugins(plugins, true);

    Int32 count = plugins->GetCount();
    for (Int32 i=0; i < count; i++) {
      BasePlugin* plugin = static_cast<BasePlugin*>(plugins->GetIndex(i));
      if (!plugin) continue;

      m_types.SetString(plugin->GetID(), plugin->GetName());
    }
  }

  Bool IsValidType(Int32 id) {
    Int32 index = 0;
    while (true) {
      Int32 paramid = m_types.GetIndexId(index);
      if (paramid == NOTOK) break;
      if (paramid == id) return true;
      index++;
    }
    return false;
  }

  Int32 GetTypeByIndex(Int32 index) {
    return m_types.GetIndexId(index);
  }

  String GetTypeName(Int32 type) {
    return m_types.GetString(type);
  }

  DependencyType GetDependencyType(Int32 active) {
    BasePlugin* plugin = FindPlugin(active, PLUGINTYPE_DEPENDENCYTYPE);
    if (plugin) {
      return DependencyType(static_cast<DEPENDENCYTYPEPLUGIN*>(plugin->GetPluginStructure()));
    }
    return DependencyType();
  }

  void CreateDialogMenu(GeDialog* dlg, Int32 base_id, Int32 active) {
    Int32 index = 0;
    while (true) {
      Int32 paramid = m_types.GetIndexId(index);
      if (paramid == NOTOK) break;

      String add;
      if (paramid == active) add = "&c&";
      dlg->MenuAddString(base_id + index, m_types.GetString(paramid) + add);

      index++;
    }
  }

} *g_typemng = nullptr;

class DependencyManager_Dialog : public GeDialog {

  typedef GeDialog super;

  DepNodeManager m_manager;
  DependencyManager_Model m_model;
  TreeViewCustomGui* m_treeview;
  BitmapButtonCustomGui* m_showempty_bmp;
  String m_title;
  Int32 m_subid;
  Int32 m_active;

  void LoadSettings() {
    Int32 default_type = g_typemng->GetTypeByIndex(0);
    BaseContainer* bc = GetWorldPluginData(PLUGIN_ID);
    if (bc == nullptr) {
      m_active = default_type;
      return;
    }

    Int32 value = bc->GetInt32(m_subid * 1000 + 0);
    Bool showempty = bc->GetInt32(m_subid * 1000 + 1);
    if (!g_typemng->IsValidType(value)) {
      value = default_type;
    }

    ActivateType(value);
    // m_manager.SetShowEmpty(showempty);
    if (m_showempty_bmp) {
      // TODO: Setting the toggle state of the bitmap button does not seem to work.
      // m_showempty_bmp->SetToggleState(showempty);
    }
  }

  void SaveSettings() {
    if (!g_typemng->IsValidType(m_active))
      m_active = g_typemng->GetTypeByIndex(0);

    BaseContainer bc;
    bc.SetInt32(m_subid * 1000 + 0, m_active);
    bc.SetBool(m_subid * 1000 + 1, m_manager.GetShowEmpty());
    SetWorldPluginData(PLUGIN_ID, bc, true);
  }

  void ActivateType(Int32 type) {
    if (!g_typemng->IsValidType(type))
      type = g_typemng->GetTypeByIndex(0);
    m_active = type;
  }

public:

  DependencyManager_Dialog()
  : super(), m_treeview(nullptr), m_showempty_bmp(nullptr), m_title("<NONE>"),
    m_subid(-1), m_active(-1) { }

  //| GeDialog Overrides

  Bool Open(Int32 subid) {
    m_subid = subid;
    return super::Open(DLG_TYPE_ASYNC, PLUGIN_ID, -1, -1, 200, 180, subid);
  }

  Bool RestoreLayout(Int32 pluginid, Int32 subid, void* secref) {
    m_subid = subid;
    return super::RestoreLayout(pluginid, subid, secref);
  }

  virtual Bool CreateLayout() {
    // Initialize pointer members.
    m_treeview = nullptr;

    // Generate the dialog's title.
    m_title = GeLoadString(IDC_DEPENDENCYMANAGER_NAME);
    if (m_subid != 0) m_title += " (" + String::IntToString(m_subid + 1) + ")";
    SetTitle(m_title);
    if (GroupBeginInMenuLine()) {
      BaseContainer bc;

      // "View" icon for hiding/unhiding empty source objects.
      bc.SetBool(BITMAPBUTTON_BUTTON, true);
      bc.SetString(BITMAPBUTTON_TOOLTIP, GeLoadString(IDC_DEPENDENCYMANAGER_SHOWEMPTY_TOOLTIP));
      bc.SetInt32(BITMAPBUTTON_ICONID1, RESOURCEIMAGE_EYEINACTIVE);
      bc.SetInt32(BITMAPBUTTON_ICONID2, RESOURCEIMAGE_EYEACTIVE);
      bc.SetBool(BITMAPBUTTON_TOGGLE, true);
      AddCustomGui(BTN_SHOWEMPTY, CUSTOMGUI_BITMAPBUTTON, "", 0, 16, 16, bc);

      // "New Window" icon.
      bc.SetBool(BITMAPBUTTON_BUTTON, true);
      bc.SetString(BITMAPBUTTON_TOOLTIP, GeLoadString(IDC_DEPENDENCYMANAGER_DUPLICATE_TOOLTIP));
      bc.SetInt32(BITMAPBUTTON_ICONID1, RESOURCEIMAGE_AMDUPLICATE);
      bc.SetBool(BITMAPBUTTON_TOGGLE, true);
      m_showempty_bmp = static_cast<BitmapButtonCustomGui*>(
          AddCustomGui(BTN_DUPLICATE, CUSTOMGUI_BITMAPBUTTON, "", 0, 16, 16, bc)
      );

      GroupEnd();
    }

    if (GroupBegin(GRP_MAIN, BFH_SCALEFIT | BFV_SCALEFIT, 0, 0, "", 0)) {
      // Initialize the parameters for a TreeViewCustomGUI which will display
      // the dependency tree.
      BaseContainer bc;
      bc.SetBool(TREEVIEW_FIXED_LAYOUT, true);  // All lines have the same height
      bc.SetBool(TREEVIEW_ALTERNATE_BG, true);  // Swap background color for each row
      bc.SetBool(TREEVIEW_NOENTERRENAME, true); // No renaming in the tree-view when pressing ENTER
      m_treeview = static_cast<TreeViewCustomGui*>(
          AddCustomGui(WDG_TREEVIEW, CUSTOMGUI_TREEVIEW, "", BFH_SCALEFIT | BFV_SCALEFIT, 0, 0, bc)
      );
      if (!m_treeview) return false;

      GroupEnd();
    }

    return true;
  }

  virtual Bool InitValues() {
    if (!super::createlayout) return false;
    m_manager.SetShowEmpty(false);
    g_typemng->Regather();
    LoadSettings();
    _UpdateMenus();
    _UpdateTreeView();
    return true;
  }

  virtual Bool Command(Int32 paramid, const BaseContainer& msg) {
    Bool handled = true;
    switch (paramid) {
      case BTN_DUPLICATE: {
        if (!g_dialogs) break;
        return OpenDialog(true);
      }
      case BTN_SHOWEMPTY:
        m_manager.SetShowEmpty(!m_manager.GetShowEmpty());
        SaveSettings();
        EventAdd();
        break;
      default:
        handled = false;
        break;
    }

    // Check if a type menu entry was hit.
    if (!handled && paramid >= MENU_TYPE_BASEID && paramid <= MENU_TYPE_MAXID) {
      Int32 index = paramid - MENU_TYPE_BASEID;
      ActivateType(g_typemng->GetTypeByIndex(index));
      SaveSettings();
      _UpdateMenus();
      EventAdd();
      handled = true;
    }

    return handled;
  }

  virtual Bool CoreMessage(Int32 msgid, const BaseContainer& msg) {
    switch (msgid) {
      case EVMSG_CHANGE:
        _UpdateTreeView();
        return true;
      default:
        break;
    }
    return false;
  }

private:

  void _UpdateTreeView() {
    if (!m_treeview) return;
    m_model.SetTreeLayout(m_treeview);

    if (m_manager.UpdateNodes(g_typemng->GetDependencyType(m_active))) {
      m_treeview->SetRoot(&m_manager, &m_model, GetActiveDocument());
    }
    else {
      m_treeview->SetRoot(nullptr, nullptr, nullptr);
    }
  }

  void _UpdateMenus() {
    String title = GeLoadString(MENU_TYPE);
    MenuFlushAll();
    MenuSubBegin(title);
    g_typemng->CreateDialogMenu(this, MENU_TYPE_BASEID, m_active);
    MenuSubEnd();
  }

};

/* DependencyManager_Command ===================================================================== */

class DependencyManager_Command : public CommandData {

public:

  //| CommandData Overrides

  virtual Bool Execute(BaseDocument* doc) {
    if (!g_dialogs) return false;

    // Retreive input information. We want to open a NEW window if the CTRL
    // qualifier is pressed, or just bring up the first one if not.
    BaseContainer state;
    GetInputState(BFM_INPUT_KEYBOARD, KEY_ESC /* some random key */, state);
    Int32 qualifier = state.GetInt32(BFM_INPUT_QUALIFIER);

    return OpenDialog(qualifier & QCTRL);
  }

  virtual Bool RestoreLayout(void* secref) {
    if (!g_dialogs) return false;

    RestoreLayoutSecret* secret = (RestoreLayoutSecret*)(secref);
    if (secret->subid < 0 || secret->subid >= DIALOG_COUNT) return false;
    return g_dialogs[secret->subid].RestoreLayout(PLUGIN_ID, secret->subid, secref);
  }

};

Bool OpenDialog(Bool next_closed) {
  Int32 dialog_index = -1;
  if (next_closed) {
    // Find a dialog which is not opened yet.
    for (Int32 i=0; i < DIALOG_COUNT; i++) {
      if (!g_dialogs[i].IsOpen()) {
        dialog_index = i;
        break;
      }
    }
  }
  if (next_closed && dialog_index < 0) {
    MessageDialog(GeLoadString(IDC_DIALOGMAXREACHED, String::IntToString(DIALOG_COUNT)));
    return false;
  }
  else if (dialog_index < 0) {
    dialog_index = 0;
  }

  return g_dialogs[dialog_index].Open(dialog_index);
}

Bool RegisterDependencyManager() {
  menu::root().AddPlugin(PLUGIN_ID);

  // Cleanup handler for the globally allocated data.
  nr::c4d::cleanup([] {
    if (g_dialogs) {
      delete [] g_dialogs;
      g_dialogs = nullptr;
    }
    if (g_typemng) {
      DeleteObj(g_typemng);
      g_typemng = nullptr;
    }
  });

  // GLobally allocate the dialogs and DependencyTypeManager.
  if (g_dialogs == nullptr) {
    g_dialogs = new DependencyManager_Dialog[DIALOG_COUNT];
    if (!g_dialogs) return false;
  }
  if (g_typemng == nullptr) {
    g_typemng = NewObjClear(DependencyTypeManager);
    if (g_typemng == nullptr) return false;
  }

  return RegisterCommandPlugin(
      PLUGIN_ID,
      GeLoadString(IDC_DEPENDENCYMANAGER_NAME),
      PLUGINFLAG_HIDEPLUGINMENU,
      raii::AutoBitmap("icons/depmanager.png"),
      GeLoadString(IDC_DEPENDENCYMANAGER_HELP),
      NewObjClear(DependencyManager_Command));
}

