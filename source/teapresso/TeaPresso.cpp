/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#include <c4d_apibridge.h>
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

Bool TvNode::PredictContextType(Int32 type) {
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

void TvNode::DrawCell(Int32 column, const TvRectangle& rect,
              const TvRectangle& real_rect, DrawInfo* drawinfo,
              const GeData& bgColor) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    (data->*plug->DrawCell)(this, column, rect, real_rect, drawinfo, bgColor);
}

Int32 TvNode::GetColumnWidth(Int32 column, GeUserArea* area) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->GetColumnWidth)(this, column, area);
}

Int32 TvNode::GetLineHeight(Int32 column, GeUserArea* area) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    return (data->*plug->GetLineHeight)(this, column, area);
}

void TvNode::CreateContextMenu(Int32 column, BaseContainer* bc) {
    TvOperatorData* data = TvGetNodeData<TvOperatorData>(this);
    const TVPLUGIN* plug = TvRetrieveTableX<TVPLUGIN>(data);
    (data->*plug->CreateContextMenu)(this, column, bc);
}

Bool TvNode::ContextMenuCall(Int32 column, Int32 command, Bool* refreshTree) {
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
    if (!data) return false;
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
    if (!data) return false;
    return data->GetBool(TVBASECONDITION_INVERT);
}

void TvNode::SetInverted(Bool inverted) {
    BaseContainer* data = GetDataInstance();
    if (!data) return;
    data->SetBool(TVBASECONDITION_INVERT, inverted);
}

Bool TvNode::GetDisplayColor(Vector* color, Bool depends) {
    if (!color) return false;
    BaseContainer* data = GetDataInstance();
    if (!data) return false;
    if (!depends) {
        *color = data->GetVector(TVBASE_COLOR);
        return true;
    }

    Int32 mode = data->GetInt32(TVBASE_COLORMODE);
    switch (mode) {
        case TVBASE_COLORMODE_USE:
            *color = data->GetVector(TVBASE_COLOR);
            return true;
        case TVBASE_COLORMODE_INHERIT: {
            TvNode* parent = GetUp();
            if (parent) return parent->GetDisplayColor(color, true);
            return false;
        }
        case TVBASE_COLORMODE_AUTO:
        default:
            return false;
    }
    return false;
}

void TvNode::SetDisplayColor(Vector color) {
    BaseContainer* data = GetDataInstance();
    if (!data) return;
    data->SetVector(TVBASE_COLOR, color);
}

TvNode* TvNode::ValidateContextSafety(TvNode* newParent, Bool checkRemoval) {
    TvNode* failedAt = nullptr;

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

        failedAt = ValidateContextSafety(nullptr);
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
            failedAt = child->ValidateContextSafety(nullptr, false);
            if (failedAt) break;
            child = child->GetNext();
        }
    }
    return failedAt;
}

TvNode* TvNode::Alloc(Int32 typeId) {
    BaseList2D* bl = (BaseList2D*) AllocListNode(typeId);
    if (!bl) return nullptr;
    if (!bl->IsInstanceOf(Tvbase)) {
        FreeListNode(bl);
        return nullptr;
    }
    return (TvNode*) bl;
}

void TvNode::Free(TvNode*& node) {
    if (node) {
        FreeListNode(node);
        node = nullptr;
    }
}


// ===========================================================================
// ===== TvOperatorData implementation =======================================
// ===========================================================================

BaseList2D* TvOperatorData::Execute(TvNode* host, TvNode* root, BaseList2D* context) {
    return context;
}

Bool TvOperatorData::AskCondition(TvNode* host, TvNode* root, BaseList2D* context) {
    return false;
}

Bool TvOperatorData::PredictContextType(TvNode* host, Int32 type) {
    return false;
}

Bool TvOperatorData::AcceptParent(TvNode* host, TvNode* other) {
    return true;
}

Bool TvOperatorData::AcceptChild(TvNode* host, TvNode* other) {
    return false;
}

Bool TvOperatorData::AllowRemoveChild(TvNode* host, TvNode* child) {
    return true;
}

void TvOperatorData::DrawCell(
            TvNode* host, Int32 column, const TvRectangle& rect,
            const TvRectangle& real_rect, DrawInfo* drawinfo,
            const GeData& bgColor) {
    if (!host || !drawinfo) return;
    GeUserArea* area = drawinfo->frame;

    if (column == TEAPRESSO_COLUMN_MAIN) {
        IconData icon;
        host->GetIcon(&icon);
        Int32 x = rect.x1;
        Int32 y = rect.y1;

        if (icon.bmp) {
            area->DrawBitmap(icon.bmp, x, y, TEAPRESSO_ICONSIZE,
                             TEAPRESSO_ICONSIZE, icon.x, icon.y, icon.w, icon.h,
                             BMP_ALLOWALPHA);
            x += TEAPRESSO_ICONSIZE + TEAPRESSO_HPADDING * 2;
        }

        String name = host->GetDisplayName();
        if (!c4d_apibridge::IsEmpty(name)) {
            area->DrawText(name, x, y);
            x += area->DrawGetTextWidth(name) + TEAPRESSO_HPADDING * 2;
        }
    }
}

Int32 TvOperatorData::GetColumnWidth(
            TvNode* host, Int32 column, GeUserArea* area) {
    Int32 width = 0;
    if (column == TEAPRESSO_COLUMN_MAIN) {
        width = TEAPRESSO_ICONSIZE + TEAPRESSO_HPADDING * 2;
        width += area->DrawGetTextWidth(host->GetDisplayName());
    }
    return width;
}

Int32 TvOperatorData::GetLineHeight(
            TvNode* host, Int32 column, GeUserArea* area) {
    Int32 height = 0;
    if (column == TEAPRESSO_COLUMN_MAIN) {
        height = area->DrawGetFontHeight();
        if (TEAPRESSO_ICONSIZE > height)
            height = TEAPRESSO_ICONSIZE;
    }
    return height;
}

void TvOperatorData::CreateContextMenu(
            TvNode* host, Int32 column, BaseContainer* bc) {
}

Bool TvOperatorData::ContextMenuCall(
            TvNode* host, Int32 column, Int32 command,
            Bool* refreshTree) {
    return false;
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
    return true;
}

Bool TvOperatorData::Init(GeListNode* node) {
    if (!node || !super::Init(node)) return false;
    TvNode* tNode = (TvNode*) node;
    BaseContainer* data = tNode->GetDataInstance();
    data->SetBool(TVBASE_ENABLED, true);
    data->SetInt32(TVBASE_COLORMODE, TVBASE_COLORMODE_INHERIT);
    data->SetVector(TVBASE_COLOR, Vector(0.4));
    data->SetBool(TVBASECONDITION_INVERT, false);
    return true;
}

Bool TvOperatorData::IsInstanceOf(const GeListNode* node, Int32 type) const {
    if (type == Tvbase) return true;
    TVPLUGIN const* table = TvRetrieveTableX<TVPLUGIN>(this);
    if (table->info & TVPLUGIN_CONDITION && type == Tvbasecondition) return true;
    return super::IsInstanceOf(node, type);
}

Bool TvOperatorData::Message(GeListNode* node, Int32 type, void* pData) {
    switch (type) {
        case MSG_DESCRIPTION_POSTSETPARAMETER:
            // Notify the tree-view that something has changed.
            SpecialEventAdd(MSG_TEAPRESSO_UPDATETREEVIEW);
            break;
        case MSG_DESCRIPTION_COMMAND:
            if (!pData) break;
            if (!OnDescriptionCommand((TvNode*) node, *((DescriptionCommand*) pData))) {
                return false;
            }
            break;
    }

    return super::Message(node, type, pData);
}

Bool TvOperatorData::GetDEnabling(
            GeListNode* node, const DescID& descid, const GeData& tData,
            DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
    Int32 id = TvDescIDLong(descid);
    BaseContainer* data = ((TvNode*) node)->GetDataInstance();
    if (!data) return false;

    switch (id) {
        case TVBASE_COLOR:
            return data->GetInt32(TVBASE_COLORMODE) == TVBASE_COLORMODE_USE;
    }
    return super::GetDEnabling(node, descid, tData, flags, itemdesc);
}


// ===========================================================================
// ===== Library Wrapper implementation ======================================
// ===========================================================================

Bool TvInstalled() {
    return TvLibrary::Get(0) != nullptr;
}

Bool TvRegisterOperatorPlugin(
            Int32 id, const String& name, Int32 info, DataAllocator* alloc,
            const String& desc, BaseBitmap* icon, Int32 disklevel,
            Int32 destFolder) {
    LIBCALL_R(TvRegisterOperatorPlugin, nullptr)(
                id, name, info, alloc, desc, icon,
                disklevel, destFolder);
}

TvNode* TvGetActiveRoot() {
    LIBCALL_R(TvGetActiveRoot, nullptr)();
}

BaseContainer* TvGetFolderContainer() {
    LIBCALL_R(TvGetFolderContainer, nullptr)();
}

TvNode* TvCreatePluginsHierarchy(const BaseContainer* bc) {
    LIBCALL_R(TvCreatePluginsHierarchy, nullptr)(bc);
}

void TvActivateAM(const AtomArray* arr) {
    if (arr) {
        ActiveObjectManager_SetObjects(ACTIVEOBJECTMODE_TEAPRESSO, *arr, 0);
    }
    else {
        ActiveObjectManager_SetMode(ACTIVEOBJECTMODE_TEAPRESSO, false);
    }
}








