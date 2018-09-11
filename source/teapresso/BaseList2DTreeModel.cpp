/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#include <c4d_apibridge.h>
#include "BaseList2DTreeModel.h"
#include "utils.h"

void BaseList2DTreeModel::BitAll(
            void* root, void* ud, void* node, Int32 bitmask, Bool on) {
    if (!node) return;
    BaseList2D* bl = (BaseList2D*) node;
    if (on) {
        bl->SetBit(bitmask);
    }
    else {
        bl->DelBit(bitmask);
    }

    void* child = GetDown(root, ud, node);
    while (child) {
        BitAll(root, ud, child, bitmask, on);
        child = GetNext(root, ud, child);
    }
}

void BaseList2DTreeModel::DeleteByBit(
            void* root, void* ud, void* node, Int32 bitmask,
            BaseDocument* doc) {
    if (!root || !node) return;
    BaseList2D* bl = (BaseList2D*) node;
    if (bl->GetBit(bitmask) && AllowRemoveNode(root, ud, bl)) {
        if (doc) doc->AddUndo(UNDOTYPE_DELETE, bl);
        bl->Remove();
        BaseList2D::Free(bl);
        return;
    }
    else {
        void* child = GetDown(root, ud, node);
        while (child) {
            void* next = GetNext(root, ud, child);
            DeleteByBit(root, ud, child, bitmask, doc);
            child = next;
        }
    }
}

void BaseList2DTreeModel::CollectByBit(
            void* root, void* ud, void* node, Int32 bitmask,
            AtomArray* arr, Bool includeChildren) {
    if (!root || !node) return;
    BaseList2D* bl = (BaseList2D*) node;
    if (bl->GetBit(bitmask) && AllowRemoveNode(root, ud, bl)) {
        arr->Append(bl);
        if (!includeChildren) return;
    }

    void* child = GetDown(root, ud, node);
    while (child) {
        void* next = GetNext(root, ud, child);
        CollectByBit(root, ud, child, bitmask, arr, includeChildren);
        child = next;
    }
}

Bool BaseList2DTreeModel::AllowRemoveNode(
            void* root, void* ud, BaseList2D* node) {
    return true;
}

BaseList2D* BaseList2DTreeModel::AskInsertObject(
            void* root, void* ud, BaseList2D* node, void** pd, Bool copy) {
    if (copy)
        return (BaseList2D*) node->GetClone(COPYFLAGS_0, nullptr);
    return node;
}

void BaseList2DTreeModel::AskInsertObjectDone(
            void* root, void* ud, BaseList2D* node, void* pd) {
}

void* BaseList2DTreeModel::GetFirst(void* root, void* ud) {
    if (!root) return nullptr;
    return GetDown(root, ud, root);
}

void* BaseList2DTreeModel::GetNext(void* root, void* ud, void* node) {
    if (!node) return nullptr;
    return ((BaseList2D*) node)->GetNext();
}

void* BaseList2DTreeModel::GetPred(void* root, void* ud, void* node) {
    if (!node) return nullptr;
    return ((BaseList2D*) node)->GetPred();
}

void* BaseList2DTreeModel::GetDown(void* root, void* ud, void* node) {
    if (!node) return nullptr;
    return ((BaseList2D*) node)->GetDown();
}

String BaseList2DTreeModel::GetName(void* root, void* ud, void* node) {
    if (!node) return "";
    return ((BaseList2D*) node)->GetName();
}

void BaseList2DTreeModel::SetName(void* root, void* ud, void* node, const c4d_apibridge::String& name) {
    if (!node) return;
    return ((BaseList2D*) node)->SetName(name);
}

Bool BaseList2DTreeModel::IsSelected(void* root, void* ud, void* node) {
    if (!node) return false;
    return ((BaseList2D*) node)->GetBit(BIT_ACTIVE);
}

Bool BaseList2DTreeModel::IsOpened(void* root, void* ud, void* node) {
    if (!node) return false;
    return ((BaseList2D*) node)->GetBit(BIT_OFOLD);
}

void BaseList2DTreeModel::Select(void* root, void* ud, void* node, Int32 mode) {
    if (!node) return;
    BaseList2D* bl = (BaseList2D*) node;
    switch (mode) {
        case SELECTION_ADD:
            bl->SetBit(BIT_ACTIVE);
            break;
        case SELECTION_SUB:
            bl->DelBit(BIT_ACTIVE);
            break;
        case SELECTION_NEW:
            BitAll(root, ud, root, BIT_ACTIVE, false);
            bl->SetBit(BIT_ACTIVE);
            break;
    }
}

void BaseList2DTreeModel::Open(void* root, void* ud, void* node, Bool mode) {
    if (!node) return;
    BaseList2D* bl = (BaseList2D*) node;
    if (mode) {
        bl->SetBit(BIT_OFOLD);
    }
    else {
        bl->DelBit(BIT_OFOLD);
    }
}

Int BaseList2DTreeModel::GetId(void* root, void* ud, void* node) {
    return (Int) node;
}

Int32 BaseList2DTreeModel::GetDragType(void* root, void* ud, void* node) {
    return DRAGTYPE_ATOMARRAY;
}

void BaseList2DTreeModel::GenerateDragArray(
            void* root, void* ud, void* node, AtomArray* arr) {
    if (!root) return;
    BaseList2D* bl = (BaseList2D*) root;
    bl->DelBit(BIT_ACTIVE);
    CollectByBit(root, ud, root, BIT_ACTIVE, arr, false);
}

Int32 BaseList2DTreeModel::AcceptDragObject(
            void* root, void* ud, void* node, Int32 dragtype, void* dragobject,
            Bool& allowCopy) {
    if (dragtype != DRAGTYPE_ATOMARRAY) {
        return 0;
    }

    allowCopy = true;
    return INSERT_AFTER | INSERT_BEFORE | INSERT_UNDER;
}

void BaseList2DTreeModel::InsertObject(
            void* root, void* ud, void* node, Int32 dragtype,
            void* dragobject, Int32 insertmode, Bool copy) {
    if (!root) return;
    if (dragtype != DRAGTYPE_ATOMARRAY) return;
    BaseList2D* bl = (BaseList2D*) (node ? node : root);

    BitAll(root, ud, root, BIT_ACTIVE, false);

    AtomArray* arr = (AtomArray*) dragobject;
    Int32 count = arr->GetCount();
    Bool openNode = false;
    for (Int32 i=0; i < count; i++) {
        BaseList2D* obj = (BaseList2D*) arr->GetIndex(i);
        if (!AllowRemoveNode(root, ud, obj)) continue;

        void* pd = nullptr;
        obj = AskInsertObject(root, ud, obj, &pd, copy);
        if (!obj) continue;

        obj->Remove();
        if (!node) {
            obj->InsertUnderLast(bl);
        }
        else {
            switch (insertmode) {
                case INSERT_AFTER:
                    obj->InsertAfter(bl);
                    break;
                case INSERT_BEFORE:
                    obj->InsertBefore(bl);
                    break;
                case INSERT_UNDER:
                    openNode = true;
                    obj->InsertUnderLast(bl);
                    break;
                default:
                    break;
            }
        }

        obj->SetBit(BIT_ACTIVE);
        AskInsertObjectDone(root, ud, obj, pd);
    }

    if (openNode) {
        Open(root, ud, node, true);
    }
}

void BaseList2DTreeModel::DeletePressed(void* root, void* ud) {
    if (!root) return;
    ((BaseList2D*) root)->DelBit(BIT_ACTIVE);
    DeleteByBit(root, ud, root, BIT_ACTIVE, nullptr);
}


