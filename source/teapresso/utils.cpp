/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#include "utils.h"
#include "TeaPresso.h"

void TvCollectByBit(LONG bitmask, BaseList2D* root, AtomArray* arr,
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
    if (root == node) return FALSE;
    GeListNode* child = root->GetDown();
    while (child) {
        if (child == node) return TRUE;
        if (TvFindChild(child, node)) return TRUE;
        child = child->GetNext();
    }
    return FALSE;
}

Bool TvFindChildDirect(GeListNode* root, GeListNode* node) {
    if (root == node) return FALSE;
    GeListNode* child = root->GetDown();
    while (child) {
        if (child == node) return TRUE;
        child = child->GetNext();
    }
    return FALSE;
}

LONG TvGetInputQualifier() {
    BaseContainer state;
    if (!GetInputState(BFM_INPUT_KEYBOARD, BFM_INPUT_QUALIFIER, state)) {
        return 0;
    }
    return state.GetLong(BFM_INPUT_QUALIFIER);
}

LONG TvGetInsertMode(LONG qual) {
    LONG mode = INSERT_UNDER;
    if (qual & QSHIFT)
        mode = INSERT_BEFORE;
    else if (qual & QCTRL)
        mode = INSERT_AFTER;
    return mode;
}

Bool TvGatherPluginList(
            BaseContainer& container, TvNode* contextNode,
            LONG insertMode, Bool* atLeastOne) {
    if (atLeastOne) *atLeastOne = FALSE;

    BasePlugin* plugin = GetFirstPlugin();
    for (; plugin; plugin=plugin->GetNext()) {
        if (plugin->GetPluginType() != PLUGINTYPE_NODE) continue;
        LONG flags = plugin->GetInfo();
        if (flags & PLUGINFLAG_HIDEPLUGINMENU) continue;

        // Check if the current plugin is a TeaPresso plugin.
        LONG pluginId = plugin->GetID();
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
            else if (atLeastOne) *atLeastOne = TRUE;
        }
        else if (atLeastOne) *atLeastOne = TRUE;

        // Add the icon-id to the name.
        name += "&i" + LongToString(pluginId);
        container.SetString(pluginId, name);
        FreeListNode(node);
    }
    return TRUE;
}

Bool TvCheckInsertContext(
            TvNode* contextNode, TvNode* newNode, LONG insertMode,
            Bool fallback) {
    if (!contextNode || !newNode) return FALSE;
    TvNode* parent = contextNode->GetUp();
    switch (insertMode) {
        case INSERT_AFTER:
        case INSERT_BEFORE:
            if (parent) return newNode->ValidateContextSafety(parent) == NULL;
            else return fallback;
        case INSERT_UNDER:
            return newNode->ValidateContextSafety(contextNode) == NULL;
    }
    return fallback;
}

Bool TvInsertNode(
            TvNode* contextNode, TvNode* newNode, LONG insertMode,
            Bool fallback) {
    if (!TvCheckInsertContext(contextNode, newNode, insertMode, fallback)) {
        return FALSE;
    }
    switch (insertMode) {
        case INSERT_AFTER:
            newNode->InsertAfter(contextNode);
            return TRUE;
        case INSERT_BEFORE:
            newNode->InsertBefore(contextNode);
            return TRUE;
        case INSERT_UNDER:
            newNode->InsertUnder(contextNode);
            return TRUE;
    }
    return FALSE;
}


Bool TvGetPluginInformation(BasePlugin* plug, String* name) {
    if (!plug) return FALSE;
    if (!name) return FALSE;

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
            if (!table) return FALSE;
            if (table->name) *name = *table->name;
            else return FALSE;
            break;
        }
        default:
            return FALSE;
        // TODO: Additional plugin-types.
    }

    return TRUE;
}

void TvFillPopupContainerBasePlugin(
            BaseContainer& dest, const AtomArray& arr, Bool hideHidden) {
    LONG count = arr.GetCount();
    if (count <= 0) return;

    for (LONG i=0; i < count; i++) {
        BasePlugin* plugin = (BasePlugin*) arr.GetIndex(i);
        if (!plugin) continue;
        if (hideHidden && plugin->GetInfo() & PLUGINFLAG_HIDE) continue;

        LONG id = plugin->GetID();
        String name = "";
        if (!TvGetPluginInformation(plugin, &name)) continue;

        if (!name.Content()) continue;
        name += "&i" + LongToString(id);
        dest.SetString(id, name);
    }
}














