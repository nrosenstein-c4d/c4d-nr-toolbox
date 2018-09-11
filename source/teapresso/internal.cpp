/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#include <c4d_apibridge.h>
#include "internal.h"
#include "TeaPresso.h"
#include "res/c4d_symbols.h"

#define Tvfolder 1030299

TvLibrary* TvLibrary::Get(Int32 offset) {
    static TvLibrary* lib = nullptr;
    CheckLib(TEAPRESSO_LIB_ID, offset, (C4DLibrary**) &lib);
    return lib;
}

// ===========================================================================
// ===== Private =============================================================
// ===========================================================================

static TvNode* TvCreatePluginsHierarchy_Recursion(const BaseContainer* bc, Int32 depth=0) {
    TvNode* root = TvNode::Alloc(Tvfolder);
    if (!root) return nullptr;
    root->SetName(bc->GetString(1));

    Int32 id;
    for (Int32 i=0; true; i++) {
        id = bc->GetIndexId(i);
        if (id == NOTOK) break;

        const GeData* data = bc->GetDataPointer(id);
        if (!data) continue;

        TvNode* node = nullptr;
        switch (data->GetType()) {
            case DA_CONTAINER:
                node = TvCreatePluginsHierarchy_Recursion(data->GetContainer(), depth + 1);
                break;
            case DA_STRING:
                node = TvNode::Alloc(id);
                if (node) node->SetName(data->GetString());
                break;
        }

        if (node) {
            node->InsertUnderLast(root);
        }
    }

    return root;
}

class TvFolderData : public TvOperatorData {

public:

    static NodeData* Alloc() { return NewObjClear(TvFolderData); }

    /* TvOperatorData Overrides */

    virtual BaseList2D* Execute(TvNode* host, TvNode* root, BaseList2D* context) {
        return nullptr;
    }

    virtual Bool AcceptParent(TvNode* host, TvNode* other) {
        return false;
    }

};


// ===========================================================================
// ===== Library Implementation ==============================================
// ===========================================================================

static TvNode* gRootNode = nullptr;
static TvLibrary lib;


static TvNode* iTvGetActiveRoot() {
    return gRootNode;
}

static BaseContainer* iTvGetFolderContainer() {
    static BaseContainer tabData;
    return &tabData;
}

static TvNode* iTvCreatePluginsHierarchy(const BaseContainer* bc) {
    if (!bc) bc = iTvGetFolderContainer();
    if (!bc) return nullptr;
    return TvCreatePluginsHierarchy_Recursion(bc);
}

static Bool iTvRegisterOperatorPlugin(
            Int32 id, const String& name, Int32 info, DataAllocator* alloc,
            const String& desc, BaseBitmap* icon, Int32 disklevel,
            Int32 destFolder) {
    if (!c4d_apibridge::IsEmpty(desc) && !RegisterDescription(id, desc)) return false;

    TVPLUGIN data;
    ClearMem(&data, sizeof(data));
    FillNodePlugin(&data, info, alloc, icon, disklevel); //, nullptr);

    data.Execute            = &TvOperatorData::Execute;
    data.AskCondition       = &TvOperatorData::AskCondition;
    data.PredictContextType = &TvOperatorData::PredictContextType;
    data.AcceptParent       = &TvOperatorData::AcceptParent;
    data.AcceptChild        = &TvOperatorData::AcceptChild;
    data.AllowRemoveChild   = &TvOperatorData::AllowRemoveChild;
    data.DrawCell           = &TvOperatorData::DrawCell;
    data.GetColumnWidth     = &TvOperatorData::GetColumnWidth;
    data.GetLineHeight      = &TvOperatorData::GetLineHeight;
    data.CreateContextMenu  = &TvOperatorData::CreateContextMenu;
    data.ContextMenuCall    = &TvOperatorData::ContextMenuCall;
    data.GetDisplayName     = &TvOperatorData::GetDisplayName;

    if (destFolder >= 0 && !(info & PLUGINFLAG_HIDEPLUGINMENU)) {
        BaseContainer* folders = iTvGetFolderContainer();

        if (destFolder == 0) {
            switch (info) {
                case TVPLUGIN_EXPRESSION:
                    destFolder = TEAPRESSO_FOLDER_EXPRESSIONS;
                    break;
                case TVPLUGIN_CONDITION:
                    destFolder = TEAPRESSO_FOLDER_CONDITIONS;
                    break;
                default:
                    destFolder = TEAPRESSO_FOLDER_GENERAL;
                    break;
            }
        }

        if (folders && destFolder > 0) {
            BaseContainer* dest = folders->GetContainerInstance(destFolder);
            if (dest) {
                dest->SetString(id, name);
            }
        }
    }

    return GeRegisterPlugin(PLUGINTYPE_NODE, id, name, &data, sizeof(data));
}


// ===========================================================================
// ===== Library Initialization ==============================================
// ===========================================================================

static Bool InitFolderContainer() {
    BaseContainer* bc = iTvGetFolderContainer();
    if (!bc) return false;

    // Register the folder icon.#
    /*
    RegisterIcon(TEAPRESSO_FOLDERICON, AutoBitmap("folder.png"));
    String icon = "&i" + String::IntToString(TEAPRESSO_FOLDERICON);
    */

    BaseContainer general;
    general.SetString(1, GeLoadString(FOLDER_GENERAL));
    bc->SetContainer(TEAPRESSO_FOLDER_GENERAL, general);

    BaseContainer conditions;
    conditions.SetString(1, GeLoadString(FOLDER_CONDITIONS));
    bc->SetContainer(TEAPRESSO_FOLDER_CONDITIONS, conditions);

    BaseContainer expressions;
    expressions.SetString(1, GeLoadString(FOLDER_EXPRESSIONS));
    bc->SetContainer(TEAPRESSO_FOLDER_EXPRESSIONS, expressions);

    BaseContainer iterators;
    iterators.SetString(1, GeLoadString(FOLDER_ITERATORS));
    bc->SetContainer(TEAPRESSO_FOLDER_ITERATORS, iterators);

    BaseContainer contexts;
    contexts.SetString(1, GeLoadString(FOLDER_CONTEXTS));
    bc->SetContainer(TEAPRESSO_FOLDER_CONTEXTS, contexts);
    return true;
}

static Bool RegisterTvFolder() {
    return TvRegisterOperatorPlugin(
            Tvfolder, "", PLUGINFLAG_HIDE | PLUGINFLAG_HIDEPLUGINMENU,
            TvFolderData::Alloc, "", AutoBitmap(RESOURCEIMAGE_TIMELINE_FOLDER1),
            0, false);
}


Bool RegisterTvLibrary() {
    if (!InitFolderContainer()) return false;

    ClearMem(&lib, sizeof(lib));
    lib.TvRegisterOperatorPlugin    = iTvRegisterOperatorPlugin;
    lib.TvGetActiveRoot             = iTvGetActiveRoot;
    lib.TvGetFolderContainer           = iTvGetFolderContainer;
    lib.TvCreatePluginsHierarchy    = iTvCreatePluginsHierarchy;

    return InstallLibrary(TEAPRESSO_LIB_ID, &lib,
                          TEAPRESSO_LIB_VERSION, sizeof(lib));
}

Bool InitTvLibrary() {
    if (!gRootNode) {
        gRootNode = TvNode::Alloc(Tvroot);
        if (!gRootNode) return false;
        GeListHead* head = GeListHead::Alloc();
        if (!head) {
            TvNode::Free(gRootNode);
            gRootNode = nullptr;
            return false;
        }
        head->InsertFirst(gRootNode);
    }
    if (!RegisterTvFolder()) return false;
    return true;
}

void FreeTvLibrary() {
    if (gRootNode) {
        GeListHead* head = gRootNode->GetListHead();
        gRootNode->Remove();
        if (head) GeListHead::Free(head);
        TvNode::Free(gRootNode);
        gRootNode = nullptr;
    }
}












