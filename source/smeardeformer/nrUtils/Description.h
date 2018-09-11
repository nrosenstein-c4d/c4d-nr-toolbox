/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * nrUtils/Description.h
 *
 * created: 2013-07-18
 * updated: 2013-07-18
 */

#ifndef NR_UTILS_DESCRIPTION_H
#define NR_UTILS_DESCRIPTION_H

    #include <c4d.h>

    namespace nr {

    /**
     * Extends a description by another description object.
     */
    Bool ExtendDescription(Description* dest, Description* source, Int32 root_id);

    /**
     * Extend a description by a resource description identifier. This
     * function will return true even if the description resource could not
     * be found. Pass a valid pointed for `loaded` if you want to check this
     * case. False is only returned on a fatal error (like a memory error).
     */
    Bool ExtendDescription(Description* dest, const String& ident, Int32 root_id, Bool* loaded=nullptr);

    /**
     * Extend a description by a resource description integral id. This
     * function will return true even if the description resource could not
     * be found. Pass a valid pointed for `loaded` if you want to check this
     * case. False is only returned on a fatal error (like a memory error).
     */
    Bool ExtendDescription(Description* dest, Int32 id, Int32 root_id, Bool* loaded=nullptr);

    } // namespace nr

#endif /* NR_UTILS_DESCRIPTION_H */
