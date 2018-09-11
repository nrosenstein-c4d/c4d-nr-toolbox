// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/defines.h>
#include <pr1mitive/debug.h>
#include <pr1mitive/helpers.h>
#include <pr1mitive/activation.h>
#include <pr1mitive/objects/BasePrimitiveData.h>
#include "res/description/Opr1m_splinechamfer.h"
#include "menu.h"

namespace pr1mitive {
namespace splines {

    class SplineChamferObject : public ObjectData {

        typedef ObjectData super;

      private:

        // Integers for dirty-checks.
        Int32 dirty_input;
        BaseObject* ref_input;

      public:

        static NodeData* alloc() { return NewObjClear(SplineChamferObject); }

        SplineChamferObject() : dirty_input(-1), ref_input(nullptr) {};

        bool check_optimize_cache(BaseObject* op, BaseObject* input);

      //
      // TagData -----------------------------------------------------------------------------------
      //

        void CheckDirty(BaseObject* op, BaseDocument* doc);

        SplineObject* GetContour(BaseObject* op, BaseDocument* doc, Float lod, BaseThread* bt);

        Bool Init(GeListNode* node);

        Bool Message(GeListNode* node, Int32 type, void* ptr);

    };

    struct PointKnot {
        PointKnot() : point(0), tl(0), tr(0), has_tangents(false), next(nullptr) {};

        Vector point;
        Vector tl;
        Vector tr;
        Bool has_tangents;
        struct PointKnot* next;
    };

    static void make_rounding(Float radius, Float ratio, Bool limit,
                              const Vector& na, const Vector& b, const Vector& nc,
                              struct PointKnot* pa, struct PointKnot* pc) {
        Float dista, distc;
        if (limit) {
            dista = na.GetLength() / 2.0;
            distc = nc.GetLength() / 2.0;
            if (dista > radius) dista = radius;
            if (distc > radius) distc = radius;
        }
        else {
            dista = distc = radius;
        }

        Vector na_ = na.GetNormalized();
        Vector nc_ = nc.GetNormalized();

        pa->point = na_ * dista + b;
        pa->tl = Vector(0);
        pa->tr = -na_ * dista / ratio;
        pa->has_tangents = true;

        pc->point = nc_ * distc + b;
        pc->tl = -nc_ * distc / ratio;
        pc->tr = Vector(0);
        pc->has_tangents = true;
    }

    static SplineObject* empty_spline() {
        return SplineObject::Alloc(0, SPLINETYPE_LINEAR);
    }

    bool SplineChamferObject::check_optimize_cache(BaseObject* op, BaseObject* input) {
        if (!input) {
            this->ref_input = nullptr;
            return false;
        }

        // Check if the input object differs from the internal reference.
        if (input != this->ref_input) {
            this->ref_input = input;
            return false;
        }

        return ! (input->IsDirty(DIRTYFLAGS_DATA) || input->IsDirty(DIRTYFLAGS_MATRIX));
    }

    void SplineChamferObject::CheckDirty(BaseObject* op, BaseDocument* doc) {
        BaseObject* input = op->GetDown();
        if (!input) {
            op->SetDirty(DIRTYFLAGS_DATA);
            return;
        }

        Bool optimize = this->check_optimize_cache(op, input);
        if (!optimize) {
            op->SetDirty(DIRTYFLAGS_DATA);
            return;
        }

        // TODO: Touch input-object and its children when cache is returned.
        if (input->IsInstanceOf(Ospline) || input->GetInfo() & OBJECT_ISSPLINE) {
            input->Touch();
        }
    }

    SplineObject* SplineChamferObject::GetContour(BaseObject* op, BaseDocument* doc, Float lod, BaseThread* bt) {
        // Obtain the input-object.
        BaseObject* input = op->GetDown();
        if (!input) {
            this->ref_input = nullptr;
            return empty_spline();
        }

        // Obtain the SplineObject to work on or return an empty sploine.
        SplineObject* src_spline = nullptr;
        Bool src_spline_owns;

        Matrix matrix;

        if (input->IsInstanceOf(Ospline)) {
            src_spline = (SplineObject*) input;
            src_spline_owns = false;
            matrix = src_spline->GetMl();
        }
        else if (input->GetInfo() & OBJECT_ISSPLINE) {
            ObjectData* data = input->GetNodeData<ObjectData>();
            if (!data) {
                PR1MITIVE_DEBUG_ERROR("Could not retrieve NodeData of input-object. End");
                return empty_spline();
            }

            OBJECTPLUGIN* vtable = (OBJECTPLUGIN*) C4DOS.Bl->RetrieveTableX(data, 0);
            if (!vtable) {
                PR1MITIVE_DEBUG_ERROR("Could not retrieve VTable of input-objects' NodeData. End");
                return empty_spline();
            }

            src_spline = (data->*(vtable->GetContour))(input, doc, lod, bt);
            if (!src_spline) {
                PR1MITIVE_DEBUG_ERROR("Contour of input-objects' VTable could not be retrieved. End");
                return empty_spline();
            }
            src_spline_owns = true;
            matrix = input->GetMl() * src_spline->GetMg();
        }
        else {
            return empty_spline();
        }

        // The input-object is touched in CheckDirty(). But when modifieng the objects' parameters
        // in the AM, it will show up sometimes. That's why we touch it once again here.
        input->Touch();

        // Make a copy of the segments from the source-object.
        Segment* segments = nullptr;
        Int32 segcount = src_spline->GetSegmentCount();
        Int32 segcount_iter = segcount;
        if (!segcount) {
            segcount_iter = 1;
            segments = new Segment[1];
            segments->cnt = src_spline->GetPointCount();
            segments->closed = src_spline->IsClosed();
        }
        else {
            segments = new Segment[segcount];
            const Segment* src_segments = src_spline->GetSegmentR();
            for (int i=0; i < segcount; i++) {
                segments[i] = src_segments[i];
            }
        }

        // Parameters.
        BaseContainer* bc = op->GetDataInstance();
        auto spldata = (const SplineData*) bc->GetCustomDataType(PR1M_SPLINECHAMFER_SPLINE, CUSTOMDATATYPE_SPLINE);
        if (!spldata) {
            PR1MITIVE_DEBUG_ERROR("SplineData could not be retrieved.");
            return empty_spline();
        }
        Float ratio  = bc->GetFloat(PR1M_SPLINECHAMFER_RATIO);
        ratio = helpers::limit_min<Float>(ratio, 0.001);

        Bool inv_selection = bc->GetBool(PR1M_SPLINECHAMFER_INVERSESELECTION);
        SelectionTag* seltag = (SelectionTag*) bc->GetLink(PR1M_SPLINECHAMFER_SELECTION, doc);
        BaseSelect* selection = nullptr;
        if (seltag) selection = seltag->GetBaseSelect();
        const Vector* src_points = src_spline->GetPointR();

        Bool limit = bc->GetBool(PR1M_SPLINECHAMFER_LIMIT);

        // Storage variables.
        Int32 dst_pcount = 0;
        Int32 src_poffset = 0;
        struct PointKnot* dst_points = nullptr;
        struct PointKnot** dst_points_next = &(dst_points);

        auto append_knot = [&dst_points_next] (struct PointKnot* p) -> void {
            *dst_points_next = p;
            dst_points_next = &(p->next);
        };

        // Iterate over all segments and process them.
        for (int i=0; i < segcount_iter; i++) {
            Segment* seg = segments + i;
            Int32 count = seg->cnt;
            Int32 j = 0;
            Int32 itercount = count;

            if (!seg->closed && seg->cnt > 0) {
                // Append the first point of the segment to the point-chain.
                struct PointKnot* p = new struct PointKnot;
                Int32 index = src_poffset;
                p->point = src_points[index];
                append_knot(p);

                // Shrink the iteration-range.
                j = 1;
                itercount--;

                // Increase the destination point-count about 2 for this and the
                // last point of the segment (after the for-loop),
                dst_pcount += 2;
            }

            for (; j < itercount; j++) {
                Int32 a, b, c;
                if (j == 0) a = count - 1;
                else a = j - 1;
                b = j;
                if (j == (count - 1)) c = 0;
                else c = j + 1;

                a += src_poffset;
                b += src_poffset;
                c += src_poffset;

                const Vector& pa = src_points[a];
                const Vector& pb = src_points[b];
                const Vector& pc = src_points[c];
                Vector na = pa - pb;
                Vector nc = pc - pb;
                Float angle = acos(Dot(na, nc) / (na.GetLength() * nc.GetLength()));

                // Verify the point to actually be rounded.
                Bool sel_ok = true;
                if (selection) {
                    sel_ok = selection->IsSelected(b);
                }
                if (inv_selection) sel_ok = !sel_ok;

                Float radius = spldata->GetPoint(angle / M_PI * 180).y;

                if (radius >= 0.0001 && sel_ok) {
                    struct PointKnot* p1 = new struct PointKnot;
                    struct PointKnot* p2 = new struct PointKnot;
                    make_rounding(radius, ratio, limit, na, pb, nc, p1, p2);

                    dst_pcount += 2;
                    seg->cnt += 1;

                    append_knot(p1);
                    append_knot(p2);
                }
                else {
                    dst_pcount++;
                    struct PointKnot* p = new struct PointKnot;
                    p->point = src_points[b];
                    append_knot(p);
                }

            }

            if (!seg->closed && seg->cnt > 0) {
                // Append the last point of the segment to the point-chain.
                struct PointKnot* p = new struct PointKnot;
                Int32 index = src_poffset + count - 1;
                p->point = src_points[index];
                append_knot(p);
            }

            // Adjust the point-index offset about the number of points
            // in the segment.
            src_poffset += count;
        }

        // Allocate the destination-object.
        SplineObject* dest = SplineObject::Alloc(0, SPLINETYPE_BEZIER);
        if (!dest) {
            PR1MITIVE_DEBUG_ERROR("Destination spline could not be allocated.");
            return empty_spline();
        }
        dest->ResizeObject(dst_pcount, segcount);

        // And set specific parameters.
        BaseContainer* dst_bc = dest->GetDataInstance();
        dst_bc->SetInt32(SPLINEOBJECT_TYPE, (Int32) SPLINETYPE_BEZIER);
        dst_bc->SetBool(SPLINEOBJECT_CLOSED, src_spline->IsClosed());

        // Set the segments to the destination-spline.
        if (segcount != 0) {
            Segment* dst_segments = dest->GetSegmentW();
            for (int i=0; i < segcount; i++) {
                dst_segments[i] = segments[i];
            }
        }

        // Obtain destination memory addresses.
        Vector* dst_wpoints = dest->GetPointW();
        Tangent* dst_tangents = dest->GetTangentW();

        // NOTE: dst_tangents could be a possible crash-causer (nullptr pointer)

        // Set the points and tangents to the destination-spline and free the PointKnots meanwhile.
        struct PointKnot* knot = dst_points;
        Int32 i = 0;
        Matrix matrix0 = matrix;
        matrix0.off = Vector(0);
        while (knot) {
            struct PointKnot* next = knot->next;

            dst_wpoints[i] = matrix * knot->point;
            if (knot->has_tangents) {
                dst_tangents[i].vl = matrix0 * knot->tl;
                dst_tangents[i].vr = matrix0 * knot->tr;
            }

            delete knot;
            knot = next;
            i++;
        }

        // Free memory.
        delete [] segments;
        if (src_spline && src_spline_owns) {
            SplineObject::Free(src_spline);
        }

        dest->Message(MSG_UPDATE);
        return dest;
    }

    Bool SplineChamferObject::Init(GeListNode* node) {
        if (!node) return false;
        BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();

        Float radius = 20;
        bc->SetFloat(PR1M_SPLINECHAMFER_RADIUS, radius);
        bc->SetFloat(PR1M_SPLINECHAMFER_RATIO, 2.41);

        // Initialize the PR1M_SPLINECHAMFER_SPLINE parameter.
        SplineData* data = SplineData::Alloc();
        data->SetRange(0, 180, 1, 0, radius, 1);
        data->MakePointBuffer(2);
        CustomSplineKnot* knot;
        knot = data->GetKnot(0);
        knot->vPos = Vector(0, radius, 0);
        knot = data->GetKnot(1);
        knot->vPos = Vector(180, radius, 0);
        GeData gedata(CUSTOMDATATYPE_SPLINE, *data);
        bc->SetParameter(PR1M_SPLINECHAMFER_SPLINE, gedata);
        SplineData::Free(data);

        bc->SetLink(PR1M_SPLINECHAMFER_SELECTION, nullptr);
        bc->SetBool(PR1M_SPLINECHAMFER_INVERSESELECTION, false);
        bc->SetBool(PR1M_SPLINECHAMFER_LIMIT, true);
        return true;
    }

    Bool SplineChamferObject::Message(GeListNode* node, Int32 type, void* ptr) {
        switch (type) {
            case MSG_DESCRIPTION_POSTSETPARAMETER: {
                auto data = (DescriptionPostSetValue*) ptr;
                auto rid = (*data->descid)[0].id;

                switch (rid) {
                    // Adapt the Spline-GUI for changing radius.
                    case PR1M_SPLINECHAMFER_RADIUS: {
                        BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();
                        auto spldata = (SplineData*) bc->GetCustomDataType(PR1M_SPLINECHAMFER_SPLINE, CUSTOMDATATYPE_SPLINE);
                        if (!spldata) return false;

                        Float t;
                        Float pre_radius;
                        Bool success = spldata->GetRange(&t, &t, &t, &t, &pre_radius, &t);
                        if (!success) {
                            PR1MITIVE_DEBUG_ERROR("Could not retrieve previous y-max value.");
                            return false;
                        }

                        Float radius = bc->GetFloat(PR1M_SPLINECHAMFER_RADIUS);
                        spldata->SetRange(0, 180, 1, 0, radius, 1);

                        Float shrink, add = 0;
                        if (pre_radius >= 0.001) shrink = radius / pre_radius;
                        else {
                            shrink = 0.0;
                            add = radius;
                        }

                        int count = spldata->GetKnotCount();
                        for (int i=0; i < count; i++) {
                            CustomSplineKnot* knot = spldata->GetKnot(i);
                            knot->vPos.y = knot->vPos.y * shrink + add;
                        }

                        break;
                    }
                    default:
                        break;
                }
                break;
            }
            default:
                break;
        }
        return super::Message(node, type, ptr);
    }

    Bool register_splinechamfer() {
        menu::root().AddPlugin(IDS_MENU_SPLINES, Opr1m_splinechamfer);
        return RegisterObjectPlugin(
            Opr1m_splinechamfer,
            GeLoadString(IDS_Opr1m_splinechamfer),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_INPUT |  OBJECT_GENERATOR | OBJECT_ISSPLINE,
            SplineChamferObject::alloc,
            "Opr1m_splinechamfer"_s,
            PR1MITIVE_ICON("Opr1m_splinechamfer"),
            0);
    }

} // end namespace splines
} // end namespace pr1mitive


