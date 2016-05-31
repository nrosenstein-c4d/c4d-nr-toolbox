// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/debug.h>
#include <pr1mitive/helpers.h>
#include <pr1mitive/splines/BaseComplexSpline.h>

#ifdef PRMT
    #pragma message("WARNING: BaseComplexSpline does not support multithreading.")
    #define BASECOMPLEXSPLIE_MULTITHREADING
#endif
#define BASECOMPLEXSPLINE_LASTRUNINFO

namespace pr1mitive {
namespace splines {

    /**
     * This thread implements computing a slice of points for
     * a complex spline.
     */
    class ContourThread : public C4DThread {

        Vector* dest_points;
        LONG start;
        LONG end;
        ComplexSplineInfo* info;
        BaseObject* op;
        BaseComplexSpline* spline;

        Bool init;
        Bool running;

    public:

        ContourThread() : C4DThread(), init(FALSE), running(FALSE) {
        }

        void Init(Vector* dest_points, LONG start, LONG end,
                  ComplexSplineInfo* info, BaseObject* op,
                  BaseComplexSpline* spline) {
            this->dest_points = dest_points;
            this->start = start;
            this->end = end;
            this->info = info;
            this->op = op;
            this->spline = spline;

            init = TRUE;
        }

        /* Override: C4DThread */
        void Main() {
            Real u;
            Real min = info->min;
            Real delta = info->delta;
            for (LONG i=start; i <= end; i++) {
                u = min + i * delta;
                dest_points[i] = spline->calc_point(op, info, u);
            }
        }

        /* Override: C4DThread */
        const CHAR* GetThreadName() {
            return "Pr1mitive-ContourThread";
        }

    };

    Bool BaseComplexSpline::init_calculation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info) {
        info->seg = bc->GetLong(PR1M_COMPLEXSPLINE_SEGMENTS);
        info->optimize = bc->GetBool(PR1M_COMPLEXSPLINE_OPTIMIZE);
        return true;
    }

    void BaseComplexSpline::post_process_calculation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info) {
        if (not info->target) return;
        BaseContainer* dest = info->target->GetDataInstance();
        dest->SetBool(SPLINEOBJECT_CLOSED, info->closed);
        dest->SetLong(SPLINEOBJECT_INTERPOLATION, bc->GetLong(SPLINEOBJECT_INTERPOLATION));
        dest->SetLong(SPLINEOBJECT_SUB, bc->GetLong(SPLINEOBJECT_SUB));
        dest->SetReal(SPLINEOBJECT_ANGLE, bc->GetReal(SPLINEOBJECT_ANGLE));
        dest->SetReal(SPLINEOBJECT_MAXIMUMLENGTH, bc->GetReal(SPLINEOBJECT_MAXIMUMLENGTH));
    }

    void BaseComplexSpline::free_calculation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info) {
    }

    Vector BaseComplexSpline::calc_point(BaseObject* op, ComplexSplineInfo* info, Real u) {
        return Vector(0);
    }

    Bool BaseComplexSpline::Init(GeListNode* node) {
        if (not node) return false;
        BaseObject* op = (BaseObject*) node;
        BaseContainer* bc = op->GetDataInstance();

        // Fill in the default-values for the Opr1m_complexcontour description.
        bc->SetLong(PR1M_COMPLEXSPLINE_SEGMENTS, 40);
        bc->SetBool(PR1M_COMPLEXSPLINE_OPTIMIZE, false);

        bc->SetLong(SPLINEOBJECT_INTERPOLATION, SPLINEOBJECT_INTERPOLATION_ADAPTIVE);
        bc->SetLong(SPLINEOBJECT_SUB, 8);
        bc->SetReal(SPLINEOBJECT_ANGLE, 5 * M_PI / 180.0);
        bc->SetReal(SPLINEOBJECT_MAXIMUMLENGTH, 5);
        return true;
    }

    SplineObject* BaseComplexSpline::GetContour(BaseObject* op, BaseDocument* doc, Real lod, BaseThread* bt) {
        if (not op) return null;
        BaseContainer* bc = op->GetDataInstance();

        // Check if actually something has been changed and return the cache if not.
        BaseObject* cache = (BaseObject*) optimize_cache(op);
        if (cache and cache->IsInstanceOf(Ospline)) return (SplineObject*) cache;

        // Initialize calculation.
        ComplexSplineInfo info;
        if (not init_calculation(op, bc, &info)) return null;

        // Make sure the segment-count is valid. Otherwise, undefined things could
        // happen.
        if (info.seg <= 0) {
            PR1MITIVE_DEBUG_ERROR("ComplexSplineInfo was not initialized to a valid state in init_calculation(). End");
            return null;
        }

        // Compute the spline-requirements.
        LONG n_points = info.seg;
        info.target = SplineObject::Alloc(n_points, info.type);
        if (not info.target) {
            PR1MITIVE_DEBUG_ERROR("SplineObject could not be allocated. End");
            return null;
        }

        // Compute the space between each segment.
        info.delta = (info.max - info.min) / info.seg;

        // Obtain the write point-pointers.
        Vector* points = info.target->GetPointW();
        if (not points) {
            PR1MITIVE_DEBUG_ERROR("Target points could not be retrieved. End");
            return null;
        }

        #ifdef BASECOMPLEXSPLINE_LASTRUNINFO
            LONG tstart = GeGetMilliSeconds();
        #endif

        Real u;
        for (LONG i=0; i < n_points; i++) {
            u = info.min + i * info.delta;
            points[i] = calc_point(op, &info, u);
        }

        #ifdef BASECOMPLEXSPLINE_LASTRUNINFO
            LONG delta = GeGetMilliSeconds() - tstart;
            String str = GeLoadString(IDS_LASTRUN, LongToString(delta));
            bc->SetString(PR1M_COMPLEXSPLINE_LASTRUN, str);
        #endif

        // Optimize points if desired.
        if (info.optimize) {
            Bool success = helpers::optimize_object(info.target, info.optimize_treshold);
            if (not success) {
                PR1MITIVE_DEBUG_ERROR("MCOMMAND_OPTIMIZE failed.");
            }
        }

        // Invoke the post-processor method.
        post_process_calculation(op, bc, &info);

        // Optional cleanups, etc.
        free_calculation(op, bc, &info);

        // Maybe post_process_object() made info.target invalid.
        if (not info.target) return null;

        // Inform the spline about the changes.
        info.target->Message(MSG_UPDATE);
        return info.target;
    }

    Bool BaseComplexSpline::GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
        // Obtain the parameter-container.
        BaseContainer *bc = ((BaseObject*)node)->GetDataInstance();

        // Enable/disable gadgets of the Osplineprimitive description.
        LONG inter;
        switch (id[0].id) {
            case SPLINEOBJECT_SUB:
                inter = bc->GetLong(SPLINEOBJECT_INTERPOLATION);
                return (inter is SPLINEOBJECT_INTERPOLATION_NATURAL) or (inter is SPLINEOBJECT_INTERPOLATION_UNIFORM);

            case SPLINEOBJECT_ANGLE:
                inter = bc->GetLong(SPLINEOBJECT_INTERPOLATION);
                return (inter is SPLINEOBJECT_INTERPOLATION_ADAPTIVE) or (inter is SPLINEOBJECT_INTERPOLATION_SUBDIV);

            case SPLINEOBJECT_MAXIMUMLENGTH:
                return bc->GetLong(SPLINEOBJECT_INTERPOLATION) is SPLINEOBJECT_INTERPOLATION_SUBDIV;
        }

        return super::GetDEnabling(node, id, t_data, flags, itemdesc);
    }

    Bool register_complexspline_base() {
        return RegisterDescription(Opr1m_complexspline, "Opr1m_complexspline");
    }

} // end namespace splines
} // end namespace pr1mitive

