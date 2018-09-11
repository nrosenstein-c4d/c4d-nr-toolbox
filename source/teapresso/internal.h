/**
 * coding: utf-8
 *
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#ifndef TV_INTERNAL_H
#define TV_INTERNAL_H

    #include <c4d.h>

    #define TEAPRESSO_LIB_ID        1030265
    #define TEAPRESSO_LIB_VERSION   1000

    class TvNode;

    /**
     * The Cinema 4D Library for the TeaPresso plugin.
     */
    struct TvLibrary : public C4DLibrary {
        Bool (*TvRegisterOperatorPlugin)(
                Int32 id, const String& name, Int32 info, DataAllocator* alloc,
                const String& desc, BaseBitmap* icon, Int32 disklevel,
                Int32 destFolder);
        TvNode* (*TvGetActiveRoot)();
        BaseContainer* (*TvGetFolderContainer)();
        TvNode* (*TvCreatePluginsHierarchy)(const BaseContainer* bc);

        static TvLibrary* Get(Int32 offset);
    };

    Bool RegisterTvLibrary();

    Bool InitTvLibrary();

    void FreeTvLibrary();

    #define LIBCALL_R(n, r) \
            TvLibrary* lib = TvLibrary::Get(LIBOFFSET(TvLibrary, n)); \
            if (!lib || !lib->n) return r; \
            return (lib->n)

    #define LIBCALL(n) \
            TvLibrary* lib = TvLibrary::Get(LIBOFFSET(TvLibrary, n)); \
            if (!lib || !lib->n) return; \
            (lib->n)

#endif /* TV_INTERNAL_H */
