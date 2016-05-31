/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#include "TeaPresso.h"
#include "BaseList2DTreeModel.h"
#include "utils.h"
#include "extensions.h"
#include "internal.h"
#include "res/c4d_symbols.h"

#include <lib_activeobjectmanager.h>

#define FULLFIT (BFH_SCALEFIT | BFV_SCALEFIT)
#define EVENT_HANDLED TRUE

#include <cstdio>

// #define ID_FORCENODECOPY 1030302

// ===========================================================================
// ===== Test Plugin Registration ============================================
// ===========================================================================

Bool SetUpTest() {
    TvNode* root = TvGetActiveRoot();
    if (!root) {
        GePrint(">>> No root node.");
        return FALSE;
    }

    TvNode* condition = TvNode::Alloc(Tvcondition);
    if (condition) {
        condition->InsertUnder(root);
        condition->SetUp();
    }
    return TRUE;
}


class TreeDialog : public GeDialog {

    typedef GeDialog super;

protected:

    LONG treeId;
    String treeName;
    BaseContainer treeData;
    TreeViewCustomGui* tree;

public:

    TreeDialog() : super(), tree(NULL), treeId(CMG_TREEVIEW) {
        treeData.SetBool(TREEVIEW_OUTSIDE_DROP, TRUE);
    }

    virtual ~TreeDialog() {}

    virtual void AttachModel() = 0;

    /* GeDialog Overrides */

    virtual Bool CreateLayout();

    virtual Bool InitValues();

    virtual Bool CoreMessage(LONG type, const BaseContainer& msg);

};

Bool TreeDialog::CreateLayout() {
    if (!super::CreateLayout()) return FALSE;
    if (GroupBegin(0, FULLFIT, 1, 0, "", 0)) {
        tree = (TreeViewCustomGui*) AddCustomGui(
                    treeId, CUSTOMGUI_TREEVIEW, treeName,
                    FULLFIT, 350, 60, treeData);
        GroupEnd();
        /*
        tree = (TreeViewCustomGui*) AddCustomGui(
                treeId, CUSTOMGUI_TREEVIEW, treeName, FULLFIT,
                350, 60, treeData); */
        // GroupEnd();
    }
    return TRUE;
}

Bool TreeDialog::InitValues() {
    if (!super::InitValues()) return FALSE;
    if (!createlayout) return TRUE;

    if (tree) {
        AttachModel();
        tree->Refresh();
    }
    return TRUE;
}

Bool TreeDialog::CoreMessage(LONG type, const BaseContainer& msg) {
    if (type == MSG_TEAPRESSO_UPDATETREEVIEW) {
        if (tree) tree->Refresh();
    }
    return GeDialog::CoreMessage(type, msg);
}

// ===========================================================================
// ===== TreeView Models =====================================================
// ===========================================================================

class TvTreeModel : public BaseList2DTreeModel {

public:

    /* BaseList2DTreeModel Overrides */

    virtual Bool AllowRemoveNode(void* root, void* ud, BaseList2D* node);

    /* TreeViewFunctions Overrides */

    virtual void DrawCell(
                void* root, void* ud, void* node, LONG column,
                DrawInfo* drawinfo, const GeData& bgColor);

    virtual LONG GetColumnWidth(
                void* root, void* ud, void* node, LONG column,
                GeUserArea* area);

    virtual LONG GetLineHeight(
                void* root, void* ud, void* node, LONG column,
                GeUserArea* area);
};

Bool TvTreeModel::AllowRemoveNode(
            void* root, void* ud, BaseList2D* node) {
    TvNode* tNode = (TvNode*) node;
    if (!tNode) return FALSE;
    TvNode* parent = tNode->GetUp();
    if (!parent) return FALSE;

    return parent->AllowRemoveChild(tNode);
}

void TvTreeModel::DrawCell(
            void* root, void* ud, void* node, LONG column,
            DrawInfo* drawinfo, const GeData& bgColor) {
    if (!root || !node) return;

    TvRectangle rr;
    rr.x1 = drawinfo->xpos;
    rr.y1 = drawinfo->ypos;
    rr.x2 = rr.x1 + drawinfo->width;
    rr.y2 = rr.y1 + drawinfo->height;

    TvRectangle rect;
    rect.x1 = rr.x1 + TEAPRESSO_HPADDING;
    rect.y1 = rr.y1 + TEAPRESSO_VPADDING;
    rect.x2 = rr.x2 - TEAPRESSO_HPADDING;
    rect.y2 = rr.y2 - TEAPRESSO_VPADDING;

    GeUserArea* area = drawinfo->frame;
    TvNode* tNode = (TvNode*) node;

    // Set default colors.
    Bool selected = IsSelected(root, ud, node);
    LONG textColor;

    if (!tNode->IsEnabled(FALSE))
        textColor = selected ? COLOR_TEXT_SELECTED_DARK : COLOR_TEXT_DISABLED;
    else if (!tNode->IsEnabled(TRUE))
        textColor = selected ? COLOR_TEXT_SELECTED_DARK : COLOR_TEXT_EDIT_DISABLED;
    else
        textColor = selected ? COLOR_TEXT_SELECTED : COLOR_TEXT;

    area->DrawSetPen(bgColor);
    area->DrawSetTextCol(textColor, COLOR_TRANS);

    tNode->DrawCell(column, rect, rr, drawinfo, bgColor);
}

LONG TvTreeModel::GetColumnWidth(
            void* root, void* ud, void* node, LONG column, GeUserArea* area) {
    if (!root || !node) return 0;
    TvNode* t_node = (TvNode*) node;
    return t_node->GetColumnWidth(column, area) + TEAPRESSO_HPADDING * 2;
}

LONG TvTreeModel::GetLineHeight(
            void* root, void* ud, void* node, LONG column, GeUserArea* area) {
    if (!root || !node) return 0;
    TvNode* t_node = (TvNode*) node;
    return t_node->GetLineHeight(column, area) + TEAPRESSO_VPADDING * 2;
}



/**
 * This is a class implementing the TreeViewFunctions interface
 * for displaying TvNode's.
 */
class TvManagerTreeModel : public TvTreeModel {

    typedef BaseList2DTreeModel super;

    TreeViewCustomGui* tree;

public:

    virtual void Attach(TreeViewCustomGui* tree);

    /* BaseList2DTreeModel Overrides */

    virtual BaseList2D* AskInsertObject(
                void* root, void* ud, BaseList2D* node, void** pd, Bool copy);

    virtual void AskInsertObjectDone(
                void* root, void* ud, BaseList2D* node, void* pd);

    /* TreeViewFunctions Overrides */

    virtual void SetName(
                void* root, void* ud, void* node, const String& name) {
        super::SetName(root, ud, node, name);
        TvUpdateTreeViews();
    }

    virtual void SelectionChanged(void* root, void* ud) {
        TvActivateAM();
        EventAdd();
    }

    virtual void DeletePressed(void* root, void* ud) {
        super::DeletePressed(root, ud);
        TvUpdateTreeViews();
    }

    virtual void CreateContextMenu(
                void* root, void* ud, void* node, LONG column,
                BaseContainer* bc);

    virtual Bool ContextMenuCall(
                void* root, void* ud, void* node, LONG column,
                LONG command);

    virtual LONG AcceptDragObject(
                void* root, void* ud, void* node, LONG dragtype,
                void* dragobject, Bool& allowCopy);

    virtual void InsertObject(
                void* root, void* ud, void* node, LONG dragtype,
                void* dragobject, LONG insertmode, Bool copy);

    virtual LONG IsChecked(void* root, void* ud, void* node, LONG column);

    virtual void SetCheck(
                void* root, void* ud, void* node, LONG column, Bool state,
                const BaseContainer& msg);

    virtual void GetBackgroundColor(
                void* root, void* ud, void* node, LONG line, GeData* col);

};

void TvManagerTreeModel::Attach(TreeViewCustomGui* tree) {
    this->tree = tree;
    if (!tree) return;
    BaseContainer layout;
    layout.SetLong(TEAPRESSO_COLUMN_MAIN, LV_USERTREE);
    layout.SetLong(TEAPRESSO_COLUMN_ENABLED, LV_CHECKBOX);
    layout.SetLong(TEAPRESSO_COLUMN_CUSTOM, LV_USER);
    tree->SetLayout(3, layout);
    tree->SetHeaderText(TEAPRESSO_COLUMN_MAIN, GeLoadString(COLUMN_MAIN));
    tree->SetHeaderText(TEAPRESSO_COLUMN_ENABLED, GeLoadString(COLUMN_ENABLED));
    tree->SetHeaderText(TEAPRESSO_COLUMN_CUSTOM, GeLoadString(COLUMN_CUSTOM));
}

BaseList2D* TvManagerTreeModel::AskInsertObject(
            void* root, void* ud, BaseList2D* node, void** pd, Bool copy) {
    if (!node) return NULL;
    if (node->GetBit(BIT_FORCE_UNOPTIMIZED)) {
        *pd = (void*) TRUE;
        copy = TRUE;
    }
    return super::AskInsertObject(root, ud, node, pd, copy);
}

void TvManagerTreeModel::AskInsertObjectDone(
            void* root, void* ud, BaseList2D* node, void* pd) {
    if (pd) {
        ((TvNode*) node)->SetUp();
    }
    node->DelBit(BIT_FORCE_UNOPTIMIZED);
}

void TvManagerTreeModel::CreateContextMenu(
            void* root, void* ud, void* node, LONG column,
            BaseContainer* bc) {
    if (!root) return;
    if (node) {
        ((TvNode*) node)->CreateContextMenu(column, bc);
    }
}

Bool TvManagerTreeModel::ContextMenuCall(
            void* root, void* ud, void* node, LONG column, LONG command) {
    if (!root) return FALSE;

    TvNode* tRoot = (TvNode*) root;
    TvNode* tNode = (TvNode*) node;

    // Check the hard-coded context entries.
    switch (command) {
        case ID_TREEVIEW_CONTEXT_RESET: {
            TvNode* child = ((TvNode*) root)->GetDown();
            while (child) {
                TvNode* next = child->GetNext();
                child->Remove();
                child = next;
            }
            TvUpdateTreeViews();
            return TRUE;
        }
        case CONTEXT_EDIT: {
            AutoAlloc<AtomArray> arr;
            TvCollectByBit(BIT_ACTIVE, (TvNode*) root, arr, TRUE);
            if (arr->GetCount() > 0) {
                EditObjectModal(*arr, GeLoadString(CONTEXT_EDIT));
                ActiveObjectManager_SetObjects(ACTIVEOBJECTMODE_TEAPRESSO, *arr, 0);
            }
            return TRUE;
        }
    }

    /* TODO: Find a better way to determine whether a plugin is intented
     * to be created or not. */

    // No command matched, try to allocate a TeaPresso node.
    TvNode* newNode = (TvNode*) AllocListNode(command);
    if (newNode && !newNode->IsInstanceOf(Tvbase)) {
        FreeListNode(newNode);
    }
    else if (newNode) {
        LONG mode = TvGetInsertMode(TvGetInputQualifier());
        TvNode* context = tNode ? tNode : tRoot;
        if (!tNode) mode = INSERT_UNDER;

        Bool inserted = FALSE;
        inserted = TvInsertNode(context, newNode, mode);
        if (!inserted) {
            GePrint("Node " + newNode->GetName() + " could not be inserted.");
            FreeListNode(newNode);
            return TRUE;
        }
        newNode->SetUp();

        // Select the new node.
        Select(root, ud, newNode, SELECTION_NEW);

        // Open all nodes in a row.
        TvNode* current = newNode;
        while (current) {
            Open(root, ud, current, TRUE);
            current = current->GetUp();
        }

        TvUpdateTreeViews();
        return TRUE;
    }

    // Ask the current node, maybe?
    if (tNode) {
        Bool refreshTree = FALSE;
        if (tNode->ContextMenuCall(column, command, &refreshTree)) {
            if (refreshTree && tree)
                TvUpdateTreeViews();
            return TRUE;
        }
    }

    // So nothing actually worked, ask the super-class.
    return super::ContextMenuCall(root, ud, node, column, command);
}

LONG TvManagerTreeModel::AcceptDragObject(
            void* root, void* ud, void* node, LONG dragtype,
            void* dragobject, Bool& allowCopy) {
    allowCopy = TRUE;
    if (dragtype != DRAGTYPE_ATOMARRAY) return 0;
    AtomArray* arr = (AtomArray*) dragobject;
    TvNode* tRoot = (TvNode*) root;
    TvNode* tNode = (TvNode*) node;
    if (tNode == tRoot) tNode = NULL;

    GePrint("AcceptDragObject: " + (tNode ? tNode->GetName() : "No Node, only Root"));

    if (!arr || arr->GetCount() <= 0) return 0;

    BitAll(root, ud, root, BIT_HIGHLIGHT, FALSE);

    // Make sure the objects in the array are all TeaPresso nodes.
    LONG count = arr->GetCount();
    LONG flags = INSERT_AFTER | INSERT_BEFORE | INSERT_UNDER;
    if (!tNode) flags &= ~INSERT_UNDER;
    for (LONG i=0; i < count; i++) {
        C4DAtom* obj = arr->GetIndex(i);
        if (!obj || !obj->IsInstanceOf(Tvbase)) {
            #ifdef VERBOSE
            GePrint("Drag object is not an instance of Tvbase");
            #endif
            return 0;
        }

        TvNode* tObj = (TvNode*) obj;
        if (tObj == tNode) {
            #ifdef VERBOSE
            GePrint("Drag object and destination equal.");
            #endif
            return 0;
        }

        TvNode* parent = tObj->GetUp();

        // If the drag-node may not be moved, disallow it.
        if (parent && !parent->AllowRemoveChild(tObj)) {
            GePrint(parent->GetName() + " does not allow " + tObj->GetName() + " to be removed.");
            return 0;
        }

        // Check if the node to be inserted is a parent of the
        // node to be inserted under. This operation is not allowed.
        if (TvFindChild(tObj, tNode)) {
            #ifdef VERBOSE
            GePrint(tNode->GetName() + " is a child of " + tObj->GetName() + ", cannot insert.");
            #endif
            return 0;
        }

        // Check if the current object would fit under the destination.
        TvNode* failedAt;
        failedAt = tObj->ValidateContextSafety(tNode ? tNode : tRoot, FALSE);
        if (failedAt) {
            #ifdef VERBOSE
            GePrint("failedAt 1 " + String(tNode ? " with destination." : " with root only."));
            #endif
            failedAt->SetBit(BIT_HIGHLIGHT);
            if (tNode) {
                flags &= ~INSERT_UNDER;
            }
            else {
                flags &= ~INSERT_AFTER;
                flags &= ~INSERT_BEFORE;
            }
        }

        // Check if the current object would fit under the parent-node
        // of the destination (so after or before the destination object).
        if (tNode) {
            parent = tNode->GetUp();
            if (parent) {
                failedAt = tObj->ValidateContextSafety(parent, FALSE);
                if (failedAt) {
                    #ifdef VERBOSE
                    GePrint("failedAt 2");
                    #endif
                    failedAt->SetBit(BIT_HIGHLIGHT);
                    flags &= ~INSERT_AFTER;
                    flags &= ~INSERT_BEFORE;
                }
            }
        }

        tObj->DelBit(BIT_HIGHLIGHT);
        if (tree) {
            GeUserArea* area = tree->GetTreeViewArea();
            if (area) area->Redraw();
        }
    }

    GePrint(">> " + LongToString(flags));
    return flags;
}

void TvManagerTreeModel::InsertObject(
            void* root, void* ud, void* node, LONG dragtype,
            void* dragobject, LONG insertmode, Bool copy) {
    BitAll(root, ud, node, BIT_HIGHLIGHT, FALSE);
    super::InsertObject(root, ud, node, dragtype, dragobject,
                        insertmode, copy);
    TvActivateAM();
    EventAdd();
}

LONG TvManagerTreeModel::IsChecked(
            void* root, void* ud, void* node, LONG column) {
    switch (column) {
        case TEAPRESSO_COLUMN_ENABLED: {
            TvNode* tNode = (TvNode*) node;
            LONG flags = LV_CHECKBOX_ENABLED;
            Bool itemEnabled = tNode->IsEnabled(FALSE);
            if (tNode->IsEnabled(TRUE) && itemEnabled) {
                flags |= LV_CHECKBOX_CHECKED;
            }
            else if (itemEnabled) {
                flags |= LV_CHECKBOX_TRISTATE;
            }
            return flags;
        }
    }
    return 0;
}

void TvManagerTreeModel::SetCheck(
            void* root, void* ud, void* node, LONG column, Bool state,
            const BaseContainer& msg) {
    switch (column) {
        case TEAPRESSO_COLUMN_ENABLED:
            return ((TvNode*) node)->SetEnabled(state);
    }
}

void TvManagerTreeModel::GetBackgroundColor(
            void* root, void* ud, void* node, LONG line, GeData* col) {
    if (!root || !node) return;
    TvNode* tNode = (TvNode*) node;
    if (tNode->GetBit(BIT_HIGHLIGHT)) {
        col->SetLong(COLOR_SYNTAX_COMMENTWRONG);
    }
    else {
        Vector color;
        if (tNode->GetDisplayColor(&color)) {
            col->SetVector(color);
        }
    }
}


class TvPluginsTreeModel : public TvTreeModel {

    typedef BaseList2DTreeModel super;

public:

    void Attach(TreeViewCustomGui* tree) {
        if (!tree) return;
        BaseContainer layout;
        layout.SetLong(TEAPRESSO_COLUMN_MAIN, LV_USERTREE);
        tree->SetLayout(1, layout);
    }

    /* TreeViewFunctions Overrides */

    virtual LONG GetDragType(void* root, void* ud, void* node) {
        // Setting the BIT_FORCE_UNOPTIMIZED notifies the other tree-view
        // to copy the dragged nodes.
        BitAll(root, ud, root, BIT_FORCE_UNOPTIMIZED, TRUE);
        return super::GetDragType(root, ud, node);
    }

    virtual LONG AcceptDragObject(
                void* root, void* ud, void* node, LONG dragtype,
                void* dragobject, Bool& allowCopy) {
        return 0;
    }

    virtual Bool DoubleClick(
            void* root, void* userdata, void* obj, LONG col,
            MouseInfo* mouseinfo) {
        return EVENT_HANDLED;
    }

    virtual void DeletePressed(void* root, void* ud) {}

    virtual void CreateContextMenu(
                void* root, void* ud, void* node, LONG column,
                BaseContainer* bc) {
        bc->FlushAll();
    }

    virtual void DrawCell(
                void* root, void* ud, void* node, LONG column,
                DrawInfo* drawinfo, const GeData& bgColor);

    virtual LONG GetColumnWidth(
                void* root, void* ud, void* node, LONG column,
                GeUserArea* area);

    virtual LONG GetLineHeight(
                void* root, void* ud, void* node, LONG column,
                GeUserArea* area);

};

void TvPluginsTreeModel::DrawCell(
            void* root, void* ud, void* node, LONG column,
            DrawInfo* drawinfo, const GeData& bgColor) {
    if (!root || !node) return;

    TvRectangle rr;
    rr.x1 = drawinfo->xpos;
    rr.y1 = drawinfo->ypos;
    rr.x2 = rr.x1 + drawinfo->width;
    rr.y2 = rr.y1 + drawinfo->height;

    TvRectangle rect;
    rect.x1 = rr.x1 + TEAPRESSO_HPADDING;
    rect.y1 = rr.y1 + TEAPRESSO_VPADDING;
    rect.x2 = rr.x2 - TEAPRESSO_HPADDING;
    rect.y2 = rr.y2 - TEAPRESSO_VPADDING;

    GeUserArea* area = drawinfo->frame;
    TvNode* tNode = (TvNode*) node;

    // Set default colors.
    Bool selected = IsSelected(root, ud, node);
    LONG textColor = selected ? COLOR_TEXT_SELECTED : COLOR_TEXT;

    area->DrawSetPen(bgColor);
    area->DrawSetTextCol(textColor, COLOR_TRANS);

    // Obtain the general plugin icon, not the instance icon.
    IconData icon;
    GetIcon(tNode->GetType(), &icon);
    if (!icon.bmp) tNode->GetIcon(&icon);

    LONG x = rect.x1;
    LONG y = rect.y1;

    if (icon.bmp) {
        area->DrawBitmap(icon.bmp, x, y, TEAPRESSO_ICONSIZE,
                         TEAPRESSO_ICONSIZE, icon.x, icon.y, icon.w, icon.h,
                         BMP_ALLOWALPHA);
    }
    else {
        area->DrawSetPen(COLOR_BGFOCUS);
        LONG w, h;
        w = h = TEAPRESSO_ICONSIZE;
        area->DrawRectangle(x, y, x + w, y + h);
    }
    x += TEAPRESSO_ICONSIZE + TEAPRESSO_HPADDING * 2;

    String name = tNode->GetName();
    if (name.Content()) {
        area->DrawText(name, x, y);
        x += area->DrawGetTextWidth(name) + TEAPRESSO_HPADDING * 2;
    }
}

LONG TvPluginsTreeModel::GetColumnWidth(
            void* root, void* ud, void* node, LONG column, GeUserArea* area) {
    if (!root || !node) return 0;
    LONG width = 0;
    if (column == TEAPRESSO_COLUMN_MAIN) {
        width = TEAPRESSO_ICONSIZE + TEAPRESSO_HPADDING * 2;
        width += area->DrawGetTextWidth(((TvNode*) node)->GetName());
    }
    return width + TEAPRESSO_HPADDING * 2;
}

LONG TvPluginsTreeModel::GetLineHeight(
            void* root, void* ud, void* node, LONG column, GeUserArea* area) {
    LONG height = 0;
    if (column == TEAPRESSO_COLUMN_MAIN) {
        height = area->DrawGetFontHeight();
        if (TEAPRESSO_ICONSIZE > height)
            height = TEAPRESSO_ICONSIZE;
    }
    return height + TEAPRESSO_VPADDING * 2;
}



// ===========================================================================
// ===== Dialogs =============================================================
// ===========================================================================

class TvManagerDialog : public TreeDialog {

    typedef TreeDialog super;

    TvManagerTreeModel model;
    TvNode* root;

public:

    TvManagerDialog() : super(), root(NULL) {
        treeData.SetBool(TREEVIEW_HAS_HEADER, TRUE);
        treeData.SetBool(TREEVIEW_ALTERNATE_BG, TRUE);
        treeData.SetBool(TREEVIEW_HIDE_LINES, FALSE);
        treeData.SetBool(TREEVIEW_RESIZE_HEADER, TRUE);
    }

    virtual ~TvManagerDialog() {}

    void SetRoot(TvNode* root) {
        this->root = root;
        if (IsOpen()) {
            AttachModel();
        }
    }

    TvNode* GetRoot() {
        return root;
    }

    /* TreeDialog Overrides */

    virtual void AttachModel() {
        model.Attach(tree);
        if (tree) {
            tree->SetRoot(root, &model, NULL);
            tree->Refresh();
        }
    }

    /* GeDialog Overrides */

    virtual Bool CreateLayout();

    virtual Bool Command(LONG type, const BaseContainer& msg);

};

Bool TvManagerDialog::CreateLayout() {
    if (GroupBeginInMenuLine()) {
        AddButton(BTN_EDITROOT, BFH_RIGHT, 0, 0, GeLoadString(BTN_EDITROOT));
        AddButton(BTN_EXECUTE, BFH_RIGHT, 0, 0, GeLoadString(BTN_EXECUTE));
        GroupEnd();
    }
    SetTitle(GeLoadString(IDC_TVMANAGER_TITLE));
    if (!super::CreateLayout()) return FALSE;
    return TRUE;
}

Bool TvManagerDialog::Command(LONG type, const BaseContainer& msg) {
    switch (type) {
        case BTN_EDITROOT:
            if (root) {
                root->SetBit(BIT_ACTIVE);
                TvActivateAM(root);
            }
            EventAdd();
            break;
        case BTN_EXECUTE: {
            BaseDocument* doc = GetActiveDocument();
            if (!doc) return TRUE;
            if (!root) {
                GePrint("Critical: No root node set.");
                return TRUE;
            }

            doc->StartUndo();
            root->Execute(NULL, NULL);
            doc->EndUndo();
            EventAdd();
            break;
        }
    };
    return TRUE;
}


class TvPluginsDialog : public TreeDialog {

    typedef TreeDialog super;

    TvPluginsTreeModel model;
    static BaseList2D* root;

public:

    static Bool StaticInit() {
        if (!root) root = TvCreatePluginsHierarchy();
        return root != NULL;
    }

    static void StaticFree() {
        if (root) BaseList2D::Free(root);
    }

    TvPluginsDialog() : super() {
        treeData.SetBool(TREEVIEW_ALTERNATE_BG, TRUE);
        treeData.SetBool(TREEVIEW_HIDE_LINES, FALSE);
    }

    /* TreeDialog Overrides */

    virtual void AttachModel() {
        model.Attach(tree);
        if (tree) {
            tree->SetRoot(root, &model, 0);
            tree->Refresh();
        }
    }

};

BaseList2D* TvPluginsDialog::root = NULL;

// ===========================================================================
// ===== Commands ============================================================
// ===========================================================================

#define ID_TVMANAGER_COMMAND 1030264

class TvManagerCommand : public CommandData {

    TvManagerDialog manager;
    TvPluginsDialog plugins;

public:

    /* CommandData Overrides */

    virtual Bool Execute(BaseDocument* doc);

    virtual Bool RestoreLayout(void* secret);

};

Bool TvManagerCommand::Execute(BaseDocument* doc) {
    manager.SetRoot(TvGetActiveRoot());
    return manager.Open(DLG_TYPE_ASYNC, ID_TVMANAGER_COMMAND, -1, -1,
                 380, 180);
}

Bool TvManagerCommand::RestoreLayout(void* secret) {
    manager.SetRoot(TvGetActiveRoot());
    return manager.RestoreLayout(ID_TVMANAGER_COMMAND, 0, secret);
}

static Bool RegisterTvManagerCommand() {
    return RegisterCommandPlugin(
            ID_TVMANAGER_COMMAND, GeLoadString(IDC_TVMANAGER_OPEN),
            PLUGINFLAG_COMMAND_HOTKEY, AutoBitmap("teapresso-manager.png"),
            GeLoadString(IDC_TVMANAGER_OPEN_HELP), gNew(TvManagerCommand));
}


#define ID_TVPLUGINS_COMMAND 1030298

class TvPluginsCommand : public CommandData {

    TvPluginsDialog plugins;

public:

    /* CommandData Overrides */

    virtual Bool Execute(BaseDocument* doc);

    virtual Bool RestoreLayout(void* secret);

};

Bool TvPluginsCommand::Execute(BaseDocument* doc) {
    return plugins.Open(DLG_TYPE_ASYNC, ID_TVPLUGINS_COMMAND, -1, -1,
                 380, 180);
}

Bool TvPluginsCommand::RestoreLayout(void* secret) {
    return plugins.RestoreLayout(ID_TVPLUGINS_COMMAND, 0, secret);
}

static Bool RegisterTvPluginsCommand() {
    return RegisterCommandPlugin(
            ID_TVPLUGINS_COMMAND, GeLoadString(IDC_TVPLUGINS_OPEN),
            PLUGINFLAG_COMMAND_HOTKEY, AutoBitmap("teapresso-plugins.png"),
            GeLoadString(IDC_TVPLUGINS_OPEN_HELP), gNew(TvPluginsCommand));
}


// ===========================================================================
// ===== ActiveObjectManager =================================================
// ===========================================================================

GeData ActiveObjectManager_TeaPressoCallback(
            const BaseContainer& msg, void* data) {
    switch (msg.GetId()) {
        case AOM_MSG_ISENABLED:
            return TRUE;
        case AOM_MSG_GETATOMLIST:
            AtomArray* arr = (AtomArray*) data;
            TvNode* root = TvGetActiveRoot();
            TvCollectByBit(BIT_ACTIVE, root, arr);
            return TRUE;
    }
    return FALSE;
}

static Bool RegisterTvActiveObjectManagerMode() {
    return ActiveObjectManager_RegisterMode(
            ACTIVEOBJECTMODE_TEAPRESSO, GeLoadString(IDC_TEAPRESSO),
            ActiveObjectManager_TeaPressoCallback);
}


#define ID_TEAPRESSOHOOK 1030283

/**
 * This scene-hook will ensure that the global root is connected
 * to the active document to show it in the attribute manager.
 */
class TeaPressoHook : public SceneHookData {

    typedef SceneHookData super;

    String hookname;

public:

    static NodeData* Alloc() { return gNew(TeaPressoHook); }

    TeaPressoHook() : hookname(GeLoadString(IDC_TVROOT)) {
    }

    /* NodeData Overrides */

    virtual void Free(GeListNode* node);

    virtual Bool Message(GeListNode* node, LONG type, void* pData);

    virtual LONG GetBranchInfo(
                GeListNode* node, BranchInfo* info, LONG max,
                GETBRANCHINFO flags);

};

void TeaPressoHook::Free(GeListNode* node) {
    TvNode* root = TvGetActiveRoot();
    GeListHead* head = root ? root->GetListHead() : NULL;
    if (head) head->SetParent(NULL);
    super::Free(node);
}

Bool TeaPressoHook::Message(GeListNode* node, LONG type, void* pData) {
    return super::Message(node, type, pData);
}

LONG TeaPressoHook::GetBranchInfo(
            GeListNode* node, BranchInfo* info, LONG max,
            GETBRANCHINFO flags) {
    LONG count = 0;
    TvNode* root = TvGetActiveRoot();
    GeListHead* head = root ? root->GetListHead() : NULL;
    if (head && max >= 1) {
        head->SetParent(node);
        info[0].head = head;
        info[0].name = &hookname;
        info[0].id = Tvbase;
        info[0].flags = BRANCHINFOFLAGS_HIDEINTIMELINE;
        count++;
    }
    return count;
}

static Bool RegisterTeaPressoHook() {
    return RegisterSceneHookPlugin(
            ID_TEAPRESSOHOOK, "TeaPresso SceneHook",
            PLUGINFLAG_HIDE | PLUGINFLAG_SCENEHOOK_NOTDRAGGABLE,
            TeaPressoHook::Alloc, 0, 0);
}


// ===========================================================================
// ===== Plugin Callbacks ====================================================
// ===========================================================================

Bool RegisterTeapresso() {
    if (!RegisterDescription(Tvbase, "Tvbase")) {
        GePrint("Critical: Failed to register Tvbase description.");
        return FALSE;
    }
    if (!RegisterDescription(Tvbasecondition, "Tvbasecondition")) {
        GePrint("Critical: Failed to registerd Tvbasecondition description.");
        return FALSE;
    }
    if (!RegisterTeaPressoHook()) {
        GePrint("Critical: Failed to register TeaPresso Hook.");
        return FALSE;
    }
    if (!RegisterTvLibrary()) {
        GePrint("Critical: Failed to register TvLibrary.");
        return FALSE;
    }
    if (!RegisterTvExtensions()) {
        GePrint("Critical: Failed to register Tv Extensions.");
        return FALSE;
    }
    if (!RegisterTvActiveObjectManagerMode()) {
        GePrint("Critical: Failed to register Active Object Manager Mode.");
        return FALSE;
    }
    if (!RegisterTvManagerCommand()) {
        GePrint("Critical: Failed to register TvManager Command.");
        return FALSE;
    }
    if (!RegisterTvPluginsCommand()) {
        GePrint("Critical: Failed to register TvPlugins Command");
        return FALSE;
    }
    if (!InitTvLibrary()) {
        GePrint("Critical: Failed to initialize TvLibrary.");
        return FALSE;
    }
    if (!TvPluginsDialog::StaticInit()) {
        GePrint("Critical: TvPluginDialog could not be initialized.");
        return FALSE;
    }

    // GePrint("TeaPresso was registered successfully.");
    return SetUpTest();
}

Bool MessageTeapresso(LONG type, void* pData) {
    switch (type) {
        case C4DPL_ENDACTIVITY:
            TvPluginsDialog::StaticFree();
            FreeTvLibrary();
            break;
    };
    return TRUE;
}
