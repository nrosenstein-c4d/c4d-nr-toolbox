/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#ifndef BASELIST2D_TREEVIEWMODEL_H
#define BASELIST2D_TREEVIEWMODEL_H

    #include "misc/legacy.h"
    #include <c4d.h>

    class BaseList2DTreeModel : public TreeViewFunctions {

    protected:

        virtual void BitAll(
                    void* root, void* ud, void* node, LONG bitmask, Bool on);

        virtual void DeleteByBit(
                    void* root, void* ud, void* node, LONG bitmask,
                    BaseDocument* doc=NULL);

        virtual void CollectByBit(
                    void* root, void* ud, void* node, LONG bitmask,
                    AtomArray* arr, Bool includeChildren=TRUE);

    public:

        virtual Bool AllowRemoveNode(void* root, void* ud, BaseList2D* node);

        virtual BaseList2D* AskInsertObject(
                    void* root, void* ud, BaseList2D* node, void** pd, Bool copy);

        virtual void AskInsertObjectDone(
                    void* root, void* ud, BaseList2D* node, void* pd);

        /* TreeViewFunctions Overrides */

        virtual void* GetFirst(void* root, void* ud);

        virtual void* GetNext(void* root, void* ud, void* node);

        virtual void* GetPred(void* root, void* ud, void* node);

        virtual void* GetDown(void* root, void* ud, void* node);

        virtual String GetName(void* root, void* ud, void* node);

        virtual void SetName(
                    void* root, void* ud, void* node, const String& name);

        virtual Bool IsSelected(void* root, void* ud, void* node);

        virtual Bool IsOpened(void* root, void* ud, void* node);

        virtual void Select(void* root, void* ud, void* node, LONG mode);

        virtual void Open(void* root, void* ud, void* node, Bool mode);

        virtual VLONG GetId(void* root, void* ud, void* node);

        virtual LONG GetDragType(void* root, void* ud, void* node);

        virtual void GenerateDragArray(
                    void* root, void* ud, void* node, AtomArray* arr);

        virtual LONG AcceptDragObject(
                    void* root, void* ud, void* node, LONG dragtype,
                    void* dragobject, Bool& allowCopy);

        virtual void InsertObject(
                    void* root, void* ud, void* node, LONG dragtype,
                    void* dragobject, LONG insertmode, Bool copy);

        virtual void DeletePressed(void* root, void* ud);

    };

#endif /* BASELIST2D_TREEVIEWMODEL_H */
