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
        Int32 start;
        Int32 end;
        ComplexSplineInfo* info;
        BaseObject* op;
        BaseComplexSpline* spline;

        Bool init;
        Bool running;

    public:

        ContourThread() : C4DThread(), init(false), running(false) {
        }

        void Init(Vector* dest_points, Int32 start, Int32 end,
                  ComplexSplineInfo* info, BaseObject* op,
                  BaseComplexSpline* spline) {
            this->dest_points = dest_points;
            this->start = start;
            this->end = end;
            this->info = info;
            this->op = op;
            this->spline = spline;

            init = true;
        }

        /* Override: C4DThread */
        void Main() {
            Float u;
            Float min = info->min;
            Float delta = info->delta;
            for (Int32 i=start; i <= end; i++) {
                u = min + i * delta;
                dest_points[i] = spline->calc_point(op, info, u);
            }
        }

        /* Override: C4DThread */
        const Char* GetThreadName() {
            return "Pr1mitive-ContourThread";
        }

    };

    Bool BaseComplexSpline::init_calculation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info) {
        info->seg = bc->GetInt32(PR1M_COMPLEXSPLINE_SEGMENTS);
        info->optimize = bc->GetBool(PR1M_COMPLEXSPLINE_OPTIMIZE);
        return true;
    }

    void BaseComplexSpline::post_process_calculation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info) {
        if (!info->target) return;
        BaseContainer* dest = info->target->GetDataInstance();
        dest->SetBool(SPLINEOBJECT_CLOSED, info->closed);
        dest->SetInt32(SPLINEOBJECT_INTERPOLATION, bc->GetInt32(SPLINEOBJECT_INTERPOLATION));
        dest->SetInt32(SPLINEOBJECT_SUB, bc->GetInt32(SPLINEOBJECT_SUB));
        dest->SetFloat(SPLINEOBJECT_ANGLE, bc->GetFloat(SPLINEOBJECT_ANGLE));
        dest->SetFloat(SPLINEOBJECT_MAXIMUMLENGTH, bc->GetFloat(SPLINEOBJECT_MAXIMUMLENGTH));
    }

    void BaseComplexSpline::free_calculation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info) {
    }

    Vector BaseComplexSpline::calc_point(BaseObject* op, ComplexSplineInfo* info, Float u) {
        return Vector(0);
    }

    Bool BaseComplexSpline::Init(GeListNode* node) {
        if (!node) return false;
        BaseObject* op = (BaseObject*) node;
        BaseContainer* bc = op->GetDataInstance();

        // Fill in the default-values for the Opr1m_complexcontour description.
        bc->SetInt32(PR1M_COMPLEXSPLINE_SEGMENTS, 40);
        bc->SetBool(PR1M_COMPLEXSPLINE_OPTIMIZE, false);

        bc->SetInt32(SPLINEOBJECT_INTERPOLATION, SPLINEOBJECT_INTERPOLATION_ADAPTIVE);
        bc->SetInt32(SPLINEOBJECT_SUB, 8);
        bc->SetFloat(SPLINEOBJECT_ANGLE, 5 * M_PI / 180.0);
        bc->SetFloat(SPLINEOBJECT_MAXIMUMLENGTH, 5);
        return true;
    }

    SplineObject* BaseComplexSpline::GetContour(BaseObject* op, BaseDocument* doc, Float lod, BaseThread* bt) {
        if (!op) return nullptr;
        BaseContainer* bc = op->GetDataInstance();

        // Check if actually something has been changed and return the cache if not.
        BaseObject* cache = (BaseObject*) optimize_cache(op);
        if (cache && cache->IsInstanceOf(Ospline)) return (SplineObject*) cache;

        // Initialize calculation.
        ComplexSplineInfo info;
        if (!init_calculation(op, bc, &info)) return nullptr;

        // Make sure the segment-count is valid. Otherwise, undefined things could
        // happen.
        if (info.seg <= 0) {
            PR1MITIVE_DEBUG_ERROR("ComplexSplineInfo was not initialized to a valid state in init_calculation(). End");
            return nullptr;
        }

        // Compute the spline-requirements.
        Int32 n_points = info.seg;
        info.target = SplineObject::Alloc(n_points, info.type);
        if (!info.target) {
            PR1MITIVE_DEBUG_ERROR("SplineObject could not be allocated. End");
            return nullptr;
        }

        // Compute the space between each segment.
        info.delta = (info.max - info.min) / info.seg;

        // Obtain the write point-pointers.
        Vector* points = info.target->GetPointW();
        if (!points) {
            PR1MITIVE_DEBUG_ERROR("Target points could not be retrieved. End");
            return nullptr;
        }

        #ifdef BASECOMPLEXSPLINE_LASTRUNINFO
            Int32 tstart = GeGetMilliSeconds();
        #endif

        Float u;
        for (Int32 i=0; i < n_points; i++) {
            u = info.min + i * info.delta;
            points[i] = calc_point(op, &info, u);
        }

        #ifdef BASECOMPLEXSPLINE_LASTRUNINFO
            Int32 delta = GeGetMilliSeconds() - tstart;
            String str = GeLoadString(IDS_LASTRUN, String::IntToString(delta));
            bc->SetString(PR1M_COMPLEXSPLINE_LASTRUN, str);
        #endif

        // Optimize points if desired.
        if (info.optimize) {
            Bool success = helpers::optimize_object(info.target, info.optimize_treshold);
            if (!success) {
                PR1MITIVE_DEBUG_ERROR("MCOMMAND_OPTIMIZE failed.");
            }
        }

        // Invoke the post-processor method.
        post_process_calculation(op, bc, &info);

        // Optional cleanups, etc.
        free_calculation(op, bc, &info);

        // Maybe post_process_object() made info.target invalid.
        if (!info.target) return nullptr;

        // Inform the spline about the changes.
        info.target->Message(MSG_UPDATE);
        return info.target;
    }

    Bool BaseComplexSpline::GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
        // Obtain the parameter-container.
        BaseContainer *bc = ((BaseObject*)node)->GetDataInstance();

        // Enable/disable gadgets of the Osplineprimitive description.
        Int32 inter;
        switch (id[0].id) {
            case SPLINEOBJECT_SUB:
                inter = bc->GetInt32(SPLINEOBJECT_INTERPOLATION);
                return (inter == SPLINEOBJECT_INTERPOLATION_NATURAL) || (inter == SPLINEOBJECT_INTERPOLATION_UNIFORM);

            case SPLINEOBJECT_ANGLE:
                inter = bc->GetInt32(SPLINEOBJECT_INTERPOLATION);
                return (inter == SPLINEOBJECT_INTERPOLATION_ADAPTIVE) || (inter == SPLINEOBJECT_INTERPOLATION_SUBDIV);

            case SPLINEOBJECT_MAXIMUMLENGTH:
                return bc->GetInt32(SPLINEOBJECT_INTERPOLATION) == SPLINEOBJECT_INTERPOLATION_SUBDIV;
        }

        return super::GetDEnabling(node, id, t_data, flags, itemdesc);
    }

    Bool register_complexspline_base() {
        return RegisterDescription(Opr1m_complexspline, "Opr1m_complexspline"_s);
    }

} // end namespace splines
} // end namespace pr1mitive

