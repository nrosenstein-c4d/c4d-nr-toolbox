// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/debug.h>
#include <pr1mitive/helpers.h>
#include <pr1mitive/activation.h>
#include <pr1mitive/shapes/BaseComplexShape.h>
#include "res/description/Opr1m_spintorus.h"
#include "menu.h"

using namespace pr1mitive::helpers;

namespace pr1mitive {
namespace shapes {

    class SpinTorusShape : public BaseComplexShape {

        typedef BaseComplexShape super;

      private:

        // Handle IDs
        static const char HANDLE_RADIUS = 0;
        static const char HANDLE_PIPERADIUS = 1;

      public:

        static NodeData* alloc() { return gNew(SpinTorusShape); }

      //
      // BaseComplexShape --------------------------------------------------------------------------
      //

        Bool init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        void free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        void post_process_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        Vector calc_point(BaseObject* op, ComplexShapeInfo* info, Real u, Real v, LONG thread_index);

      //
      // BasePrimitiveData
      //

        LONG get_handle_count(BaseObject* op);

        Bool get_handle(BaseObject* op, LONG handle, HandleInfo* info);

        void set_handle(BaseObject* op, LONG handle, HandleInfo* info);

      //
      // ObjectData --------------------------------------------------------------------------------
      //

        Bool Init(GeListNode* node);

    };

    struct SpinTorusData {
        Real radius;
        Real pradius;
    };

    Bool SpinTorusShape::init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        if (not super::init_calculation(op, bc, info)) return false;
        info->umin = info->vmin = 0;
        info->umax = info->vmax = 2 * M_PI;

        struct SpinTorusData* data = new struct SpinTorusData;
        if (not data) return false;
        info->data = (void*) data;
        data->pradius = bc->GetReal(PR1M_SPINTORUS_PIPERADIUS);
        data->radius = bc->GetReal(PR1M_SPINTORUS_RADIUS) + data->pradius;;
        return true;
    }

    void SpinTorusShape::free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        auto data = (struct SpinTorusData*) info->data;
        delete data;
    }

    void SpinTorusShape::post_process_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        // Some normales have to be reversed.
        LONG vseg2 = info->vseg;
        CPolygon* polys = info->target->GetPolygonW();
        LONG polycount = info->target->GetPolygonCount();
        UVWTag* uvw = info->uvw_dest;
        UVWHandle uvwhandle ;
        if (uvw) uvwhandle = uvw->GetDataAddressW();
        else uvwhandle = null;

        for (LONG i=0; i < polycount; i++) {
            if ((i % vseg2) < (vseg2 / 2 - 0.5)) {
                CPolygon* poly = polys + i;
                CPolygon  ref  = *poly;
                Bool is_tri = ref.c == ref.d;

                // Distunguish between quads and tris and reverse the polygon.
                if (is_tri) {
                    poly->a = ref.c;
                    poly->b = ref.b;
                    poly->c = ref.a;
                    poly->d = poly->c;
                }
                else {
                    poly->a = ref.d;
                    poly->b = ref.c;
                    poly->c = ref.b;
                    poly->d = ref.a;
                }

                // Inverse UVW normals as well if available.
                if (uvwhandle) {
                    UVWStruct uvws, uvwr;
                    uvw->Get(uvwhandle, i, uvws);
                    uvwr = uvws;

                    if (is_tri) {
                        uvws.a = uvwr.c;
                        uvws.b = uvwr.b;
                        uvws.c = uvwr.a;
                        uvws.d = uvws.c;
                    }
                    else {
                        uvws.a = uvwr.d;
                        uvws.b = uvwr.c;
                        uvws.c = uvwr.b;
                        uvws.d = uvwr.a;
                    }

                    uvw->Set(uvwhandle, i, uvws);
                }

            }
        }
    }

    Vector SpinTorusShape::calc_point(BaseObject* op, ComplexShapeInfo* info, Real u, Real v, LONG thread_index) {
        auto data = (struct SpinTorusData*) info->data;
        Real r = data->pradius;
        Real R = data->radius;
        Real x = (R + r * (Cos(u / 2) * Sin(v) - Sin(u / 2) * Sin(2 * v))) * Cos(u);
        Real y = (R + r * (Cos(u / 2) * Sin(v) - Sin(u / 2) * Sin(2 * v))) * Sin(u);
        Real z = r * (Sin(u / 2) * Sin(v) + Cos(u / 2) * Sin(2 * v));
        return Vector(x, z, y);
    }

    LONG SpinTorusShape::get_handle_count(BaseObject* op) {
        return 2;
    }

    Bool SpinTorusShape::get_handle(BaseObject* op, LONG handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        Vector& point = info->position;
        info->direction = Vector(1, 0, 0);
        Real radius = bc->GetReal(PR1M_SPINTORUS_RADIUS);

        switch (handle) {
            case HANDLE_RADIUS:
                point.x = radius;
                break;
            case HANDLE_PIPERADIUS:
                point.x = radius + bc->GetReal(PR1M_SPINTORUS_PIPERADIUS) * 2;
                break;
            default:
                return false;
        }

        return true;
    }

    void SpinTorusShape::set_handle(BaseObject* op, LONG handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        Vector& point = info->position;

        switch (handle) {
            case HANDLE_RADIUS:
                bc->SetReal(PR1M_SPINTORUS_RADIUS, limit_min<Real>(point.x, 0));
                break;

            case HANDLE_PIPERADIUS:
                Real radius = limit_min<Real>(bc->GetReal(PR1M_SPINTORUS_RADIUS), 0);
                Real pradius = (point.x - radius) * 0.5;
                bc->SetReal(PR1M_SPINTORUS_PIPERADIUS, pradius);
                break;
        }
    }

    Bool SpinTorusShape::Init(GeListNode* node) {
        if (not super::Init(node)) return false;
        BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();
        bc->SetReal(PR1M_SPINTORUS_RADIUS, 100);
        bc->SetReal(PR1M_SPINTORUS_PIPERADIUS, 20);
        bc->SetLong(PR1M_COMPLEXSHAPE_USEGMENTS, 40);
        bc->SetLong(PR1M_COMPLEXSHAPE_VSEGMENTS, 40);
        return true;
    }

    Bool register_spintorus() {
        menu::root().AddPlugin(IDS_MENU_OBJECTS, Opr1m_spintorus);
        return RegisterObjectPlugin(
            Opr1m_spintorus,
            GeLoadString(IDS_Opr1m_spintorus),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_GENERATOR,
            SpinTorusShape::alloc,
            "Opr1m_spintorus",
            PR1MITIVE_ICON("Opr1m_spintorus"), 0);
    }

} // end namespace shapes
} // end namespace pr1mitive

