/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * nrUtils/Description.cpp
 *
 * created: 2013-07-18
 * updated: 2013-07-18
 */

#include "Description.h"

namespace nr {

Bool ExtendDescription(Description* dest, Description* source, Int32 root_id) {
    void* handle = source->BrowseInit();
    if (!handle) {
        return false; // memory error
    }

    const BaseContainer* itemdesc = nullptr;
    DescID id, groupid;
    while (source->GetNext(handle, &itemdesc, id, groupid)) {
        if (DESCID_ROOT == itemdesc->GetInt32(DESC_PARENTID)) {
            // TODO: Check if this is legal and correct.
            const_cast<BaseContainer*>(itemdesc)->SetInt32(DESC_PARENTID, root_id);
        }
        dest->SetParameter(id, *itemdesc, groupid);
    }

    source->BrowseFree(handle);
    return true;
}

Bool ExtendDescription(Description* dest, const String& ident, Int32 root_id, Bool* loaded) {
    if (loaded) *loaded = false;
    AutoAlloc<Description> source;
    if (!source) {
        return false;
    }
    if (!source->LoadDescription(ident)) {
        return true;
    }
    if (!ExtendDescription(dest, source, root_id)) {
        return false;
    }
    if (loaded) *loaded = true;
    return true;
}

Bool ExtendDescription(Description* dest, Int32 id, Int32 root_id, Bool* loaded) {
    if (loaded) *loaded = false;
    AutoAlloc<Description> source;
    if (!source) {
        return false;
    }
    if (!source->LoadDescription(id)) {
        return true;
    }
    if (!ExtendDescription(dest, source, root_id)) {
        return false;
    }
    if (loaded) *loaded = true;
    return true;
}


} // namespace nr

