/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#ifndef TV_UTILS_H
#define TV_UTILS_H

    #include "misc/legacy.h"
    #include <c4d.h>

    class TvNode;

    /**
     * Helper to retrieve the #NodeData of a #GeListNode independently
     * of the API version (since the API has changed and breaked backwards
     * compatibility with code written for previous versions in R17).
     */
    //@{
    template<typename CAST>
    inline CAST* TvGetNodeData(GeListNode* node) {
        #if API_VERSION >= 17000
            return node->GetNodeData<CAST>();
        #else
            return static_cast<CAST*>(node->GetNodeData());
        #endif
    }

    template<typename CAST>
    inline CAST const* TvGetNodeData(GeListNode const* node) {
        #if API_VERSION >= 17000
            return node->GetNodeData<CAST>();
        #else
            return static_cast<CAST const*>(node->GetNodeData());
        #endif
    }
    //@}

    /**
     * Collect all elements in the hierarchy started by *root* in
     * the AtomArray *arr* that have the specified bit set.
     */
    void TvCollectByBit(LONG bitmask, BaseList2D* root, AtomArray* arr,
                        Bool includeChildren=TRUE);

    /**
     * Retrieve a NodeData's virtual function table.
     *
     * @param Type The plugin-type, eg. NODEPLUGIN, OBJECTPLUGIN, etc.
     * @param data The NodeData to retrieve the virtual function table
     *        for.
     * @return The virtual function table for the passed NodeData.
     */
    // @{
    template<typename Type>
    inline Type* TvRetrieveTableX(NodeData* data) {
        return (Type*) C4DOS.Bl->RetrieveTableX((NodeData*) data, 0);
    }

    template<typename Type>
    inline Type const* TvRetrieveTableX(NodeData const* data) {
        return (Type const*) C4DOS.Bl->RetrieveTableX((NodeData*) data, 0);
    }
    //@}

    /**
     * Retrieve a BaseList2D's virtual function table.
     *
     * @param Type The plugin-type, eg. NODEPLUGIN, OBJECTPLUGIN, etc.
     * @param data The BaseList2D to retrieve the virtual function table
     *        for.
     * @return The virtual function table for the passed BaseList2D.
     */
    //@{
    template<typename Type>
    inline Type* TvRetrieveTableX(GeListNode* obj) {
        NodeData* data = TvGetNodeData<NodeData>(obj);
        return (Type*) C4DOS.Bl->RetrieveTableX(data, 0);
    }

    template<typename Type>
    inline Type const* TvRetrieveTableX(GeListNode const* obj) {
        NodeData const* data = TvGetNodeData<NodeData>(obj);
        return (Type const*) C4DOS.Bl->RetrieveTableX((NodeData*) data, 0);
    }
    //@}


    /**
     * @param root The root node to start from.
     * @param node The node to search for.
     * @return TRUE when *node* is a child of *root*, but not if
     * *root* and *node* equal.
     */
    Bool TvFindChild(GeListNode* root, GeListNode* node);

    /**
     * @param root The root node to start from.
     * @param node The node to search for.
     * @return TRUE if *node* is a direct child of *root*.
     */
    Bool TvFindChildDirect(GeListNode* root, GeListNode* node);

    /**
     * @return The current keyboard input qualifier.
     */
    LONG TvGetInputQualifier();

    /**
     * @return The INSERT_Mode depending on the passed qualifier.
     */
    LONG TvGetInsertMode(LONG qual);

    /**
     * Fills the passed BaseContainer with plugin-IDs and plugin-Names
     * of registered TeaPresso plugins, including disabling and icon
     * informatino.
     *
     * @param container The container to be filled.
     * @param contextNode When set, insertMode must be given as well.
     *        The function will then disable (adding "&d&") items in
     *        the container based on the context.
     * @param insertMode How a new node of the plugins is going to
     *        be inserted. There is also an INSERT_ABOVE constant,
     *        defined in the TeaPresso utils header.
     * @param atLeastOne If not NULL is passed, the pointed memory
     *        location will be set to TRUE if at least one plugin
     *        is allowed to be inserted.
     */
    Bool TvGatherPluginList(
                BaseContainer& container, TvNode* contextNode=NULL,
                LONG insertMode=0, Bool* atLeastOne=NULL);

    /**
     * @param contextNode The node relative to the new location
     *        of the new node.
     * @param newNode The new node.
     * @param insertMode The insertion-mode.
     * @return TRUE if *newNode* is allowed to be inserted the way
     * defined with *insertMode* in relation to *contextNode*.
     */
    Bool TvCheckInsertContext(
                TvNode* contextNode, TvNode* newNode, LONG insertMode,
                Bool fallback=FALSE);

    /**
     * @param contextNode The node relative to the new location
     *        of the new node.
     * @param newNode The new node.
     * @param insertMode The insertion-mode.
     * @return TRUE if the node was inserted, FALSE if not.
     */
    Bool TvInsertNode(
                TvNode* contextNode, TvNode* newNode, LONG insertMode,
                Bool fallback=FALSE);

    /**
     * Return static information from a plugin-type.
     */
    Bool TvGetPluginInformation(BasePlugin* plug, String* name);

    /**
     * Fills a BaseContainer for a popup-menu (see ShowPopupMenu()),
     * fro  an AtomArray of BasePlugin's.
     *
     * @param dest The destination container.
     * @param arr The AtomArray of BasePlugin's. The function does
     *        not check whether it actually is a BasePlugin, so it will
     *        crash when it contains some other kind of object.
     * @param hideHidden TRUE when hidden plugins should be skipped.
     * @return The selected BasePlugin or NULL.
     */
    void TvFillPopupContainerBasePlugin(
                BaseContainer& dest, const AtomArray& arr,
                Bool hideHidden=TRUE);

    /**
     * Retrieve the LONG id from a DescID.
     *
     * @param descid The DescID to retrieve the LONG from.
     * @return A LONG of the id.
     */
    inline LONG TvDescIDLong(const DescID& descid) {
        return descid[descid.GetDepth() - 1].id;
    }

#endif /* TV_UTILS_H */
