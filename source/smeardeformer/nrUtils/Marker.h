/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * nrUtils/Marker.h
 *
 * created: 2013-07-18
 * updated: 2013-07-18
 */

#ifndef NR_UTILS_MARKER_H
#define NR_UTILS_MARKER_H

    #include <c4d.h>

    namespace nr {

    /**
     * This function ensures that an object has a GeMarker attached. This
     * is ie. necessary when you want to retrieve the GUID of this object.
     * Returns false on failure, true if the object already had a marker
     * attached or it was attached.
     */
    Bool EnsureGeMarker(BaseObject* op);

    } // namespace nr

#endif /* NR_UTILS_MARKER_H */
