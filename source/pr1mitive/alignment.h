// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.
//
/*  This header basically takes over the description used by the Axis Alignment-Tag. Although
    the description is registered by the Axis Alignment-Tag, this module uses constants defined
    in its description and even provides functionality for initializing a container with the
    default values for the parameters. This module is used internally by the Axis Alignment-Tag.
    It does only care about the x, Y and Z Alignment descriptions. The offset-parameter in the
    Axis Alignment-Tag description is handled by the tag itself. */

#include <pr1mitive/defines.h>
#include "res/description/Tpr1m_axisalign.h"

#ifndef PR1MITIVE_ALIGNMENT_H
#define PR1MITIVE_ALIGNMENT_H

namespace pr1mitive {
namespace alignment {

    // Class for computing the Axis Aligned Bounding Box of a set of objects.
    class AABBHelper {

        Bool   is_init;
        Bool   detailed_measuring;
        MinMax mm;

      public:

        Matrix translation;

        // Construct an uninitialized AABBHelper object.
        AABBHelper() : is_init(false), detailed_measuring(false) {};

        AABBHelper(Matrix translation) : is_init(false), translation(translation), detailed_measuring(false) {};

        // Expand the AABB by a point in 3d space.
        void expand(const Vector& point);

        // Expand the AABB by the passed object.
        void expand(BaseObject* op, Bool recursive=false);

        // Specified if `expand_object()` should operate sepcial on
        // point-objects.
        void set_detailed_measuring(Bool detailed) {
            detailed_measuring = detailed;
        }

        // Obtain the results by storing it into the passed references.
        inline void get_result(Vector& bbmin, Vector& bbmax) const {
            bbmin = mm.GetMax();
            bbmax = mm.GetMin();
        }

        // Returns the midpoint of the AABB. Should only be called when
        // the AABB is initialized.
        inline Vector midpoint() const {
            return (mm.GetMax() + mm.GetMin()) * 0.5;
        }

        // Returns the size of the AABB. Should only be called when
        // the AABB is initialized.
        inline Vector size() const {
            return (mm.GetMax() - mm.GetMin()) * 0.5;
        }

    };

} // end alignment
} // end namespace pr1mitive

#endif // PR1MITIVE_ALIGNMENT_H

