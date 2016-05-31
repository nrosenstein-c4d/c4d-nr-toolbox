// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/debug.h>
#include <pr1mitive/helpers.h>

namespace pr1mitive {
namespace helpers {

    Bool make_cube_selections(int n, BaseSelect** selections) {

        // Compute the number of faces for each horizontal side, which includes
        // the cover and the bottom, top and right-handed rounding.
        int pcount_side = (int) pow(n, 2.0) * 2 + n * 3 + 1;
        int polycount = pcount_side * 4 + 2;

        // Iterate over the first four selections and select the covers
        // respectively.
        for (int i=0; i < 4; i++)
            // The covers index is the side-offset + the number of roundings
            // (offset about the bottom-rounding).
            selections[i]->Select(pcount_side * i + n);

        // The top and bottom cover are the last two polygons on the cube.
        selections[4]->Select(polycount - 2);
        selections[5]->Select(polycount - 1);

        // We don't fill in any data in the roundings-selections when no roundings
        // are present.
        if (n <= 0) return true;

        // The BaseSelect::SelectAll() method deselects all previous elements before
        // selecting the passed range. That's why we have to make our own customg
        // selector-function.
        auto select_range = [] (BaseSelect* s, int min, int max) {
            for (int i=min; i <= max; i++) {
                s->Select(i);
            }
        };

        // We now iterate over the the 4 horizontal sides of the cube and fill out
        // the rounding-selections.
        int min, max;
        int offset;
        for (int i=0; i < 4; i++) {
            offset = pcount_side * i;

            // Bottom rounding
            min = offset + 0;
            max = offset + n - 1;
            select_range(selections[6], min, max);

            // Top rounding
            min = offset + n + 1;
            max = offset + n + n;
            select_range(selections[7], min, max);

            // The right-handed (vertical) rounding is aligned differently.
            for (int j=0; j < n; j++) {
                offset = pcount_side * i + n * 3 + 1 + (n * 2 + 1) * j;

                // Corner selections for the bottom and top rounding.
                min = offset - n;
                max = offset - 1;
                select_range(selections[6], min, max);

                min = offset + 1;
                max = offset + n;
                select_range(selections[7], min, max);

                // Right-handed (vertical) selection + corner.
                min = offset - n;
                max = offset + n;
                select_range(selections[8], min, max);
            }
        }

        return true;
    }

    UVWTag* make_planar_uvw(int useg, int vseg, Bool inverse_normals, Bool flipx, Bool flipy) {
        int polys = useg * vseg;
        if (polys <= 0 or useg <= 0 or vseg <= 0) return null;

        UVWTag* tag = UVWTag::Alloc(polys);
        if (not tag) {
            PR1MITIVE_DEBUG_ERROR("Could not allocated UVW-Tag. End of procedure");
            return null;
        }
        UVWHandle uvwhandle = tag->GetDataAddressW();

        int poly_i = 0;
        Real inv_u = 1.0 / useg;
        Real inv_v = 1.0 / vseg;
        UVWStruct uvw;
        for (int i=0; i < useg; i++) {
            for (int j=0; j < vseg; j++, poly_i++) {
                Real a = (Real) i;
                Real b = (Real) j;
                Real c = (Real) i + 1;
                Real d = (Real) j + 1;

                if (inverse_normals) {
                    uvw.a = Vector((c * inv_u), (b * inv_v), 0);
                    uvw.b = Vector((c * inv_u), (d * inv_v), 0);
                    uvw.c = Vector((a * inv_u), (d * inv_v), 0);
                    uvw.d = Vector((a * inv_u), (b * inv_v), 0);
                }
                else {
                    uvw.d = Vector((c * inv_u), (b * inv_v), 0);
                    uvw.c = Vector((c * inv_u), (d * inv_v), 0);
                    uvw.b = Vector((a * inv_u), (d * inv_v), 0);
                    uvw.a = Vector((a * inv_u), (b * inv_v), 0);
                }

                if (flipx) {
                    uvw.a.x = 1.0 - uvw.a.x;
                    uvw.b.x = 1.0 - uvw.b.x;
                    uvw.c.x = 1.0 - uvw.c.x;
                    uvw.d.x = 1.0 - uvw.d.x;
                }
                if (flipy) {
                    uvw.a.y = 1.0 - uvw.a.y;
                    uvw.b.y = 1.0 - uvw.b.y;
                    uvw.c.y = 1.0 - uvw.c.y;
                    uvw.d.y = 1.0 - uvw.d.y;
                }

                tag->Set(uvwhandle, poly_i, uvw);
            }
        }

        return tag;
    }

    Bool optimize_object(BaseObject* op, Real treshold) {
        // Allocate a container to fill in the information.
        BaseContainer bc;
        bc.SetReal(MDATA_OPTIMIZE_TOLERANCE, treshold);
        bc.SetBool(MDATA_OPTIMIZE_POINTS, true);
        bc.SetBool(MDATA_OPTIMIZE_POLYGONS, true);
        bc.SetBool(MDATA_OPTIMIZE_UNUSEDPOINTS, true);

        // Create the modeling data and fill in information.
        ModelingCommandData mdata;
        mdata.op = op;
        mdata.bc = &bc;
        return SendModelingCommand(MCOMMAND_OPTIMIZE, mdata);
    }

    BaseContainer* find_submenu(BaseContainer* menu, const String& ident) {
        if (!menu) return NULL;
        LONG i = 0;
        while (TRUE) {
            LONG idx = menu->GetIndexId(i);
            if (idx == NOTOK) break;

            GeData* data = menu->GetIndexData(i++);
            if (!data || data->GetType() != DA_CONTAINER) continue;

            BaseContainer* sub = data->GetContainer();
            if (!sub) continue;
            if (sub->GetString(MENURESOURCE_SUBTITLE) == ident) return sub;
        }
        return NULL;
    }

} // end namespace helpers
} // end namespace pr1mitive


