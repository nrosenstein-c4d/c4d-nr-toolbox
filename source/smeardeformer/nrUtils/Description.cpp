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

Bool ExtendDescription(Description* dest, Description* source, LONG root_id) {
    void* handle = source->BrowseInit();
    if (!handle) {
        return FALSE; // memory error
    }

    const BaseContainer* itemdesc = NULL;
    DescID id, groupid;
    while (source->GetNext(handle, &itemdesc, id, groupid)) {
        if (DESCID_ROOT == itemdesc->GetLong(DESC_PARENTID)) {
            // TODO: Check if this is legal and correct.
            const_cast<BaseContainer*>(itemdesc)->SetLong(DESC_PARENTID, root_id);
        }
        dest->SetParameter(id, *itemdesc, groupid);
    }

    source->BrowseFree(handle);
    return TRUE;
}

Bool ExtendDescription(Description* dest, const String& ident, LONG root_id, Bool* loaded) {
    if (loaded) *loaded = FALSE;
    AutoAlloc<Description> source;
    if (!source) {
        return FALSE;
    }
    if (!source->LoadDescription(ident)) {
        return TRUE;
    }
    if (!ExtendDescription(dest, source, root_id)) {
        return FALSE;
    }
    if (loaded) *loaded = TRUE;
    return TRUE;
}

Bool ExtendDescription(Description* dest, LONG id, LONG root_id, Bool* loaded) {
    if (loaded) *loaded = FALSE;
    AutoAlloc<Description> source;
    if (!source) {
        return FALSE;
    }
    if (!source->LoadDescription(id)) {
        return TRUE;
    }
    if (!ExtendDescription(dest, source, root_id)) {
        return FALSE;
    }
    if (loaded) *loaded = TRUE;
    return TRUE;
}


} // namespace nr

