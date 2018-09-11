/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#include <c4d_apibridge.h>
#include "utils.h"
#include "TeaPresso.h"

void TvCollectByBit(Int32 bitmask, BaseList2D* root, AtomArray* arr,
                    Bool includeChildren) {
    if (!root) return;
    if (root->GetBit(bitmask)) {
        arr->Append(root);
        if (!includeChildren) return;
    }
    BaseList2D* child = (BaseList2D*) root->GetDown();
    while (child) {
        TvCollectByBit(bitmask, child, arr, includeChildren);
        child = (BaseList2D*) child->GetNext();
    }
}

Bool TvFindChild(GeListNode* root, GeListNode* node) {
    if (root == node) return false;
    GeListNode* child = root->GetDown();
    while (child) {
        if (child == node) return true;
        if (TvFindChild(child, node)) return true;
        child = child->GetNext();
    }
    return false;
}

Bool TvFindChildDirect(GeListNode* root, GeListNode* node) {
    if (root == node) return false;
    GeListNode* child = root->GetDown();
    while (child) {
        if (child == node) return true;
        child = child->GetNext();
    }
    return false;
}

Int32 TvGetInputQualifier() {
    BaseContainer state;
    if (!GetInputState(BFM_INPUT_KEYBOARD, BFM_INPUT_QUALIFIER, state)) {
        return 0;
    }
    return state.GetInt32(BFM_INPUT_QUALIFIER);
}

Int32 TvGetInsertMode(Int32 qual) {
    Int32 mode = INSERT_UNDER;
    if (qual & QSHIFT)
        mode = INSERT_BEFORE;
    else if (qual & QCTRL)
        mode = INSERT_AFTER;
    return mode;
}

Bool TvGatherPluginList(
            BaseContainer& container, TvNode* contextNode,
            Int32 insertMode, Bool* atLeastOne) {
    if (atLeastOne) *atLeastOne = false;

    BasePlugin* plugin = GetFirstPlugin();
    for (; plugin; plugin=plugin->GetNext()) {
        if (plugin->GetPluginType() != PLUGINTYPE_NODE) continue;
        Int32 flags = plugin->GetInfo();
        if (flags & PLUGINFLAG_HIDEPLUGINMENU) continue;

        // Check if the current plugin is a TeaPresso plugin.
        Int32 pluginId = plugin->GetID();
        TvNode* node = (TvNode*) AllocListNode(pluginId);
        if (!node) continue;
        if (!node->IsInstanceOf(Tvbase)) {
            FreeListNode(node)
            continue;
        }

        // Retrieve the plugin structure and the plugin's name.
        TVPLUGIN* table = (TVPLUGIN*) plugin->GetPluginStructure();
        String name = *table->name;

        // Check if the plugin would be allowed to be inserted
        // at the current context.
        if (contextNode && insertMode != 0) {
            if (!TvCheckInsertContext(contextNode, node, insertMode)) {
                name += "&d&";
            }
            else if (atLeastOne) *atLeastOne = true;
        }
        else if (atLeastOne) *atLeastOne = true;

        // Add the icon-id to the name.
        name += "&i" + String::IntToString(pluginId);
        container.SetString(pluginId, name);
        FreeListNode(node);
    }
    return true;
}

Bool TvCheckInsertContext(
            TvNode* contextNode, TvNode* newNode, Int32 insertMode,
            Bool fallback) {
    if (!contextNode || !newNode) return false;
    TvNode* parent = contextNode->GetUp();
    switch (insertMode) {
        case INSERT_AFTER:
        case INSERT_BEFORE:
            if (parent) return newNode->ValidateContextSafety(parent) == nullptr;
            else return fallback;
        case INSERT_UNDER:
            return newNode->ValidateContextSafety(contextNode) == nullptr;
    }
    return fallback;
}

Bool TvInsertNode(
            TvNode* contextNode, TvNode* newNode, Int32 insertMode,
            Bool fallback) {
    if (!TvCheckInsertContext(contextNode, newNode, insertMode, fallback)) {
        return false;
    }
    switch (insertMode) {
        case INSERT_AFTER:
            newNode->InsertAfter(contextNode);
            return true;
        case INSERT_BEFORE:
            newNode->InsertBefore(contextNode);
            return true;
        case INSERT_UNDER:
            newNode->InsertUnder(contextNode);
            return true;
    }
    return false;
}


Bool TvGetPluginInformation(BasePlugin* plug, String* name) {
    if (!plug) return false;
    if (!name) return false;

    void* data = plug->GetPluginStructure();
    switch (plug->GetPluginType()) {
        case PLUGINTYPE_SHADER:
        case PLUGINTYPE_MATERIAL:
        case PLUGINTYPE_OBJECT:
        case PLUGINTYPE_TAG:
        case PLUGINTYPE_VIDEOPOST:
        case PLUGINTYPE_SCENEHOOK:
        case PLUGINTYPE_NODE:
        case PLUGINTYPE_CTRACK:
        case PLUGINTYPE_PREFS: {
            NODEPLUGIN* table = (NODEPLUGIN*) data;
            if (!table) return false;
            if (table->name) *name = *table->name;
            else return false;
            break;
        }
        default:
            return false;
        // TODO: Additional plugin-types.
    }

    return true;
}

void TvFillPopupContainerBasePlugin(
            BaseContainer& dest, const AtomArray& arr, Bool hideHidden) {
    Int32 count = arr.GetCount();
    if (count <= 0) return;

    for (Int32 i=0; i < count; i++) {
        BasePlugin* plugin = (BasePlugin*) arr.GetIndex(i);
        if (!plugin) continue;
        if (hideHidden && plugin->GetInfo() & PLUGINFLAG_HIDE) continue;

        Int32 id = plugin->GetID();
        String name = "";
        if (!TvGetPluginInformation(plugin, &name)) continue;

        if (c4d_apibridge::IsEmpty(name)) continue;
        name += "&i" + String::IntToString(id);
        dest.SetString(id, name);
    }
}














