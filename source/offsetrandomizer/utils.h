/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#define __LEGACY_API
#include <c4d.h>

/**
 * This class keeps track of the dirtycount of a :C4DAtom: object which
 * makes it easy to check if the object has changed. The object being
 * counted is stored only to check if the object changed. This ensures
 * to not run into deallocated memory and to reset the dirtycount if
 * the object has changed.
 */
class DirtyCounter {

    /**
     * The flag to check for the object.
     */
    DIRTYFLAGS dirtyFlag;

    /**
     * The dirtycount returned the last time the object was
     * checked.
     */
    Int32 dirtyCount;

    /**
     * A reference to the object previously used to compare the
     * dirtycount.
     */
    C4DAtom* previousRef;

    public:

    /**
     * Create a new :DirtyCounter: object with the passed flag.
     */
    DirtyCounter(DIRTYFLAGS df=DIRTYFLAGS_DATA) : previousRef(NULL) {
        SetDirtyFlag(df);
    }

    /**
     * Sets the dirtyflag, resetting the dirtycount if the flag is
     * different to the original flag.
     * @param df The new flag.
     */
    void SetDirtyFlag(DIRTYFLAGS df) {
        if (dirtyFlag != df) {
            dirtyCount = -1;
            dirtyFlag = df;
        }
    }

    /**
     * Compares the passed objects dirtycount. Returns TRUE when the
     * objects dirtycount has changed, FALSE if not.
     * @param ref The object to check.
     * @return TRUE if the object has changed or its dirtycount differs,
     * FALSE if not.
     */
    Bool HasChanged(C4DAtom* ref) {
        Int32 dc = -1;
        if (ref) dc = ref->GetDirty(dirtyFlag);
        if (previousRef != ref) {
            previousRef = ref;
            dirtyCount = dc;
            return TRUE;
        }
        if (dirtyCount != dc) {
            dirtyCount = dc;
            return TRUE;
        }
        return FALSE;
    }

};

/**
 * Returns the cache of an object used by Cinema 4D.
 */
BaseObject* GetObjectCache(BaseObject* op) {
    BaseObject* cache = op->GetDeformCache();
    if (!cache) cache = op->GetCache();
    return cache;
}

/**
 * Returns the next node by counting *index* to the right.
 */
GeListNode* GetNextNodeByIndex(GeListNode* node, Int32 count, Int32 instanceof=0) {
    Int32 index = 0;
    while (node) {
        if (node->IsInstanceOf(instanceof)) {
            if (index >= count) return node;
            else index++;
        }
        node = node->GetNext();
    }
    return NULL;
}




