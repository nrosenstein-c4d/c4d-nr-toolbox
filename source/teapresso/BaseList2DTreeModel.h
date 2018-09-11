/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#ifndef BASELIST2D_TREEVIEWMODEL_H
#define BASELIST2D_TREEVIEWMODEL_H

    #include <c4d.h>
    #include <c4d_apibridge.h>

    class BaseList2DTreeModel : public TreeViewFunctions {

    protected:

        virtual void BitAll(
                    void* root, void* ud, void* node, Int32 bitmask, Bool on);

        virtual void DeleteByBit(
                    void* root, void* ud, void* node, Int32 bitmask,
                    BaseDocument* doc=nullptr);

        virtual void CollectByBit(
                    void* root, void* ud, void* node, Int32 bitmask,
                    AtomArray* arr, Bool includeChildren=true);

    public:

        virtual Bool AllowRemoveNode(void* root, void* ud, BaseList2D* node);

        virtual BaseList2D* AskInsertObject(void* root, void* ud, BaseList2D* node, void** pd, Bool copy);

        virtual void AskInsertObjectDone(void* root, void* ud, BaseList2D* node, void* pd);

        /* TreeViewFunctions Overrides */

        virtual void* GetFirst(void* root, void* ud) override;

        virtual void* GetNext(void* root, void* ud, void* node) override;

        virtual void* GetPred(void* root, void* ud, void* node) override;

        virtual void* GetDown(void* root, void* ud, void* node) override;

        virtual String GetName(void* root, void* ud, void* node) override;

        virtual void SetName(void* root, void* ud, void* node, const c4d_apibridge::String& name) override;

        virtual Bool IsSelected(void* root, void* ud, void* node) override;

        virtual Bool IsOpened(void* root, void* ud, void* node) override;

        virtual void Select(void* root, void* ud, void* node, Int32 mode) override;

        virtual void Open(void* root, void* ud, void* node, Bool mode) override;

        virtual Int GetId(void* root, void* ud, void* node) override;

        virtual Int32 GetDragType(void* root, void* ud, void* node) override;

        virtual void GenerateDragArray(void* root, void* ud, void* node, AtomArray* arr) override;

        virtual Int32 AcceptDragObject(void* root, void* ud, void* node, Int32 dragtype,
                                       void* dragobject, Bool& allowCopy) override;

        virtual void InsertObject(void* root, void* ud, void* node, Int32 dragtype,
                                  void* dragobject, Int32 insertmode, Bool copy) override;

        virtual void DeletePressed(void* root, void* ud) override;

    };

#endif /* BASELIST2D_TREEVIEWMODEL_H */
