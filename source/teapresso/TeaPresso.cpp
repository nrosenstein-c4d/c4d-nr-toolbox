/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#include "TeaPresso.h"
#include "utils.h"
#include "internal.h"

#include "res/c4d_symbols.h"

// #define VERBOSE

// ===========================================================================
// ===== TvNode implementation ===============================================
// ===========================================================================

BaseList2D* TvNode::Execute(TvNode* root, BaseList2D* context) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->Execute)(this, root, context);
}

Bool TvNode::AskCondition(TvNode* root, BaseList2D* context) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->AskCondition)(this, root, context);
}

Bool TvNode::PredictContextType(LONG type) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->PredictContextType)(this, type);
}

Bool TvNode::AcceptParent(TvNode* other) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->AcceptParent)(this, other);
}

Bool TvNode::AcceptChild(TvNode* other) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->AcceptChild)(this, other);
}

Bool TvNode::AllowRemoveChild(TvNode* child) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->AllowRemoveChild)(this, child);
}

void TvNode::DrawCell(LONG column, const TvRectangle& rect,
              const TvRectangle& real_rect, DrawInfo* drawinfo,
              const GeData& bgColor) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    (data->*plug->DrawCell)(this, column, rect, real_rect, drawinfo, bgColor);
}

LONG TvNode::GetColumnWidth(LONG column, GeUserArea* area) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->GetColumnWidth)(this, column, area);
}

LONG TvNode::GetLineHeight(LONG column, GeUserArea* area) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->GetLineHeight)(this, column, area);
}

void TvNode::CreateContextMenu(LONG column, BaseContainer* bc) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    (data->*plug->CreateContextMenu)(this, column, bc);
}

Bool TvNode::ContextMenuCall(LONG column, LONG command, Bool* refreshTree) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->ContextMenuCall)(this, column, command, refreshTree);
}

String TvNode::GetDisplayName() const {
    TvOperatorData const* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->GetDisplayName)(this);
}

Bool TvNode::SetUp() {
    return Message(MSG_TEAPRESSO_SETUP);
}

Bool TvNode::IsEnabled(Bool checkParents) {
    BaseContainer* data = GetDataInstance();
    if (!data) return FALSE;
    Bool enabled = data->GetBool(TVBASE_ENABLED);
    if (!enabled || !checkParents) return enabled;

    TvNode* parent = GetUp();
    if (parent) return parent->IsEnabled();
    return enabled;
}


void TvNode::SetEnabled(Bool enabled) {
    BaseContainer* data = GetDataInstance();
    if (!data) return;
    data->SetBool(TVBASE_ENABLED, enabled);
}

Bool TvNode::IsInverted() const {
    const BaseContainer* data = GetDataInstance();
    if (!data) return FALSE;
    return data->GetBool(TVBASECONDITION_INVERT);
}

void TvNode::SetInverted(Bool inverted) {
    BaseContainer* data = GetDataInstance();
    if (!data) return;
    data->SetBool(TVBASECONDITION_INVERT, inverted);
}

Bool TvNode::GetDisplayColor(Vector* color, Bool depends) {
    if (!color) return FALSE;
    BaseContainer* data = GetDataInstance();
    if (!data) return FALSE;
    if (!depends) {
        *color = data->GetVector(TVBASE_COLOR);
        return TRUE;
    }

    LONG mode = data->GetLong(TVBASE_COLORMODE);
    switch (mode) {
        case TVBASE_COLORMODE_USE:
            *color = data->GetVector(TVBASE_COLOR);
            return TRUE;
        case TVBASE_COLORMODE_INHERIT: {
            TvNode* parent = GetUp();
            if (parent) return parent->GetDisplayColor(color, TRUE);
            return FALSE;
        }
        case TVBASE_COLORMODE_AUTO:
        default:
            return FALSE;
    }
    return FALSE;
}

void TvNode::SetDisplayColor(Vector color) {
    BaseContainer* data = GetDataInstance();
    if (!data) return;
    data->SetVector(TVBASE_COLOR, color);
}

TvNode* TvNode::ValidateContextSafety(TvNode* newParent, Bool checkRemoval) {
    TvNode* failedAt = NULL;

    #ifdef VERBOSE
    String pre = String(__FUNCTION__) + ": ";
    String dna = " does not accept ";
    String tRm = " to be removed from its children.";
    String tAp = " as parent.";
    String tAc = " as child.";
    #endif

    if (newParent) {
        TvNode* parent = GetUp();
        TvNode* pred = GetPred();

        if (checkRemoval && parent) {
            if (!parent->AllowRemoveChild(this)) {
                #ifdef VERBOSE
                GePrint(pre + parent->GetName() + dna + GetName() + tRm);
                #endif
                return this;
            }
        }
        if (!AcceptParent(newParent)) {
            #ifdef VERBOSE
            GePrint(pre + GetName() + dna + newParent->GetName() + tAp);
            #endif
            return this;
        }
        if (!newParent->AcceptChild(this)) {
            #ifdef VERBOSE
            GePrint(pre + newParent->GetName() + dna + GetName() + tAc);
            #endif
            return this;
        }

        Remove();
        InsertUnder(newParent);

        failedAt = ValidateContextSafety(NULL);
        Remove();
        if (pred) InsertAfter(pred);
        else if (parent) InsertUnder(parent);
    }
    else {
        TvNode* child = GetDown();
        while (child) {
            if (!child->AcceptParent(this)) {
                return child;
            }
            if (!AcceptChild(child)) return child;
            failedAt = child->ValidateContextSafety(NULL, FALSE);
            if (failedAt) break;
            child = child->GetNext();
        }
    }
    return failedAt;
}

TvNode* TvNode::Alloc(LONG typeId) {
    BaseList2D* bl = (BaseList2D*) AllocListNode(typeId);
    if (!bl) return NULL;
    if (!bl->IsInstanceOf(Tvbase)) {
        FreeListNode(bl);
        return NULL;
    }
    return (TvNode*) bl;
}

void TvNode::Free(TvNode*& node) {
    if (node) {
        FreeListNode(node);
        node = NULL;
    }
}


// ===========================================================================
// ===== TvOperatorData implementation =======================================
// ===========================================================================

BaseList2D* TvOperatorData::Execute(TvNode* host, TvNode* root, BaseList2D* context) {
    return context;
}

Bool TvOperatorData::AskCondition(TvNode* host, TvNode* root, BaseList2D* context) {
    return FALSE;
}

Bool TvOperatorData::PredictContextType(TvNode* host, LONG type) {
    return FALSE;
}

Bool TvOperatorData::AcceptParent(TvNode* host, TvNode* other) {
    return TRUE;
}

Bool TvOperatorData::AcceptChild(TvNode* host, TvNode* other) {
    return FALSE;
}

Bool TvOperatorData::AllowRemoveChild(TvNode* host, TvNode* child) {
    return TRUE;
}

void TvOperatorData::DrawCell(
            TvNode* host, LONG column, const TvRectangle& rect,
            const TvRectangle& real_rect, DrawInfo* drawinfo,
            const GeData& bgColor) {
    if (!host || !drawinfo) return;
    GeUserArea* area = drawinfo->frame;

    if (column == TEAPRESSO_COLUMN_MAIN) {
        IconData icon;
        host->GetIcon(&icon);
        LONG x = rect.x1;
        LONG y = rect.y1;

        if (icon.bmp) {
            area->DrawBitmap(icon.bmp, x, y, TEAPRESSO_ICONSIZE,
                             TEAPRESSO_ICONSIZE, icon.x, icon.y, icon.w, icon.h,
                             BMP_ALLOWALPHA);
            x += TEAPRESSO_ICONSIZE + TEAPRESSO_HPADDING * 2;
        }

        String name = host->GetDisplayName();
        if (name.Content()) {
            area->DrawText(name, x, y);
            x += area->DrawGetTextWidth(name) + TEAPRESSO_HPADDING * 2;
        }
    }
}

LONG TvOperatorData::GetColumnWidth(
            TvNode* host, LONG column, GeUserArea* area) {
    LONG width = 0;
    if (column == TEAPRESSO_COLUMN_MAIN) {
        width = TEAPRESSO_ICONSIZE + TEAPRESSO_HPADDING * 2;
        width += area->DrawGetTextWidth(host->GetDisplayName());
    }
    return width;
}

LONG TvOperatorData::GetLineHeight(
            TvNode* host, LONG column, GeUserArea* area) {
    LONG height = 0;
    if (column == TEAPRESSO_COLUMN_MAIN) {
        height = area->DrawGetFontHeight();
        if (TEAPRESSO_ICONSIZE > height)
            height = TEAPRESSO_ICONSIZE;
    }
    return height;
}

void TvOperatorData::CreateContextMenu(
            TvNode* host, LONG column, BaseContainer* bc) {
}

Bool TvOperatorData::ContextMenuCall(
            TvNode* host, LONG column, LONG command,
            Bool* refreshTree) {
    return FALSE;
}

String TvOperatorData::GetDisplayName(const TvNode* host) const {
    if (!host) return "";
    NodeData* data = (NodeData*) this;
    const TVPLUGIN* table = TvRetrieveTableX<TVPLUGIN>(data);
    String name = host->GetName();
    if (table->name)
        name += " (" + *table->name + ")";
    return name;
}

Bool TvOperatorData::OnDescriptionCommand(
            TvNode* host, const DescriptionCommand& data) {
    return TRUE;
}

Bool TvOperatorData::Init(GeListNode* node) {
    if (!node || !super::Init(node)) return FALSE;
    TvNode* tNode = (TvNode*) node;
    BaseContainer* data = tNode->GetDataInstance();
    data->SetBool(TVBASE_ENABLED, TRUE);
    data->SetLong(TVBASE_COLORMODE, TVBASE_COLORMODE_INHERIT);
    data->SetVector(TVBASE_COLOR, Vector(0.4));
    data->SetBool(TVBASECONDITION_INVERT, FALSE);
    return TRUE;
}

Bool TvOperatorData::IsInstanceOf(const GeListNode* node, LONG type) const {
    if (type == Tvbase) return TRUE;
    TVPLUGIN const* table = TvRetrieveTableX<TVPLUGIN>(this);
    if (table->info & TVPLUGIN_CONDITION && type == Tvbasecondition) return TRUE;
    return super::IsInstanceOf(node, type);
}

Bool TvOperatorData::Message(GeListNode* node, LONG type, void* pData) {
    switch (type) {
        case MSG_DESCRIPTION_POSTSETPARAMETER:
            // Notify the tree-view that something has changed.
            SpecialEventAdd(MSG_TEAPRESSO_UPDATETREEVIEW);
            break;
        case MSG_DESCRIPTION_COMMAND:
            if (!pData) break;
            if (!OnDescriptionCommand((TvNode*) node, *((DescriptionCommand*) pData))) {
                return FALSE;
            }
            break;
    }

    return super::Message(node, type, pData);
}

Bool TvOperatorData::GetDEnabling(
            GeListNode* node, const DescID& descid, const GeData& tData,
            DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
    LONG id = TvDescIDLong(descid);
    BaseContainer* data = ((TvNode*) node)->GetDataInstance();
    if (!data) return FALSE;

    switch (id) {
        case TVBASE_COLOR:
            return data->GetLong(TVBASE_COLORMODE) == TVBASE_COLORMODE_USE;
    }
    return super::GetDEnabling(node, descid, tData, flags, itemdesc);
}


// ===========================================================================
// ===== Library Wrapper implementation ======================================
// ===========================================================================

Bool TvInstalled() {
    return TvLibrary::Get(0) != NULL;
}

Bool TvRegisterOperatorPlugin(
            LONG id, const String& name, LONG info, DataAllocator* alloc,
            const String& desc, BaseBitmap* icon, LONG disklevel,
            LONG destFolder) {
    LIBCALL_R(TvRegisterOperatorPlugin, NULL)(
                id, name, info, alloc, desc, icon,
                disklevel, destFolder);
}

TvNode* TvGetActiveRoot() {
    LIBCALL_R(TvGetActiveRoot, NULL)();
}

BaseContainer* TvGetFolderContainer() {
    LIBCALL_R(TvGetFolderContainer, NULL)();
}

TvNode* TvCreatePluginsHierarchy(const BaseContainer* bc) {
    LIBCALL_R(TvCreatePluginsHierarchy, NULL)(bc);
}

void TvActivateAM(const AtomArray* arr) {
    if (arr) {
        ActiveObjectManager_SetObjects(ACTIVEOBJECTMODE_TEAPRESSO, *arr, 0);
    }
    else {
        ActiveObjectManager_SetMode(ACTIVEOBJECTMODE_TEAPRESSO, FALSE);
    }
}








