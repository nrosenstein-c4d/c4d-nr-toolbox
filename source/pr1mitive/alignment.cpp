// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/helpers.h>
#include <pr1mitive/alignment.h>

namespace pr1mitive {
namespace alignment {

    void AABBHelper::expand(const Vector& point) {
        if (is_init) mm.AddPoint(translation * point);
        else {
            mm.Init(translation * point);
            is_init = true;
        }
    }

    void AABBHelper::expand(BaseObject* op, Bool recursive) {
        Matrix mg = op->GetMg();

        if (detailed_measuring && op->IsInstanceOf(Opoint)) {
            const Vector* points = ((PointObject*)op)->GetPointR();
            if (!points) return;

            LONG count = ((PointObject*)op)->GetPointCount();
            for (LONG i=0; i < count; i++) {
                expand(points[i]);
            }
        }
        else {
            Vector rad = op->GetRad();
            Vector mp  = op->GetMp();
            Vector bbmin = mp - rad;
            Vector bbmax = mp + rad;

            expand(mg * bbmin);
            expand(mg * Vector(bbmin.x, bbmin.y, bbmax.z));
            expand(mg * Vector(bbmax.x, bbmin.y, bbmax.z));
            expand(mg * Vector(bbmax.x, bbmin.y, bbmin.z));

            // Top 4 points.
            expand(mg * bbmax);
            expand(mg * Vector(bbmax.x, bbmax.y, bbmin.z));
            expand(mg * Vector(bbmin.x, bbmax.y, bbmin.z));
            expand(mg * Vector(bbmin.x, bbmax.y, bbmax.z));
        }

        if (recursive) {
            for (BaseObject* child=op->GetDown(); child; child=child->GetNext()) {
                expand(child, true);
            }
        }
    }

} // end namespace alignment
} // end namespace pr1m

