/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * nrUtils/Marker.cpp
 *
 * created: 2013-07-18
 * updated: 2013-07-18
 */

#include "Marker.h"

namespace nr {

Bool EnsureGeMarker(BaseObject* op) {
    /*
     * The Cinema 4D API documentation states that there is possiblity
     * that an object does not have a marker. We better go the safe route
     * and really make sure it does have one.
     */
    const GeMarker* marker = &op->GetMarker();
    if (!marker) {
        GeMarker* marker = GeMarker::Alloc();
        if (marker) {
            op->SetMarker(*marker);
            GeMarker::Free(marker);
            return TRUE;
        }
    }
    return marker != NULL;
}

} // namespace nr
