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

        static NodeData* alloc() { return NewObjClear(SpinTorusShape); }

      //
      // BaseComplexShape --------------------------------------------------------------------------
      //

        Bool init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        void free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        void post_process_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        Vector calc_point(BaseObject* op, ComplexShapeInfo* info, Float u, Float v, Int32 thread_index);

      //
      // BasePrimitiveData
      //

        Int32 get_handle_count(BaseObject* op);

        Bool get_handle(BaseObject* op, Int32 handle, HandleInfo* info);

        void set_handle(BaseObject* op, Int32 handle, HandleInfo* info);

      //
      // ObjectData --------------------------------------------------------------------------------
      //

        Bool Init(GeListNode* node);

    };

    struct SpinTorusData {
        Float radius;
        Float pradius;
    };

    Bool SpinTorusShape::init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        if (!super::init_calculation(op, bc, info)) return false;
        info->umin = info->vmin = 0;
        info->umax = info->vmax = 2 * M_PI;

        struct SpinTorusData* data = new struct SpinTorusData;
        if (!data) return false;
        info->data = (void*) data;
        data->pradius = bc->GetFloat(PR1M_SPINTORUS_PIPERADIUS);
        data->radius = bc->GetFloat(PR1M_SPINTORUS_RADIUS) + data->pradius;;
        return true;
    }

    void SpinTorusShape::free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        auto data = (struct SpinTorusData*) info->data;
        delete data;
    }

    void SpinTorusShape::post_process_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        // Some normales have to be reversed.
        Int32 vseg2 = info->vseg;
        CPolygon* polys = info->target->GetPolygonW();
        Int32 polycount = info->target->GetPolygonCount();
        UVWTag* uvw = info->uvw_dest;
        UVWHandle uvwhandle ;
        if (uvw) uvwhandle = uvw->GetDataAddressW();
        else uvwhandle = nullptr;

        for (Int32 i=0; i < polycount; i++) {
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

    Vector SpinTorusShape::calc_point(BaseObject* op, ComplexShapeInfo* info, Float u, Float v, Int32 thread_index) {
        auto data = (struct SpinTorusData*) info->data;
        Float r = data->pradius;
        Float R = data->radius;
        Float x = (R + r * (Cos(u / 2) * Sin(v) - Sin(u / 2) * Sin(2 * v))) * Cos(u);
        Float y = (R + r * (Cos(u / 2) * Sin(v) - Sin(u / 2) * Sin(2 * v))) * Sin(u);
        Float z = r * (Sin(u / 2) * Sin(v) + Cos(u / 2) * Sin(2 * v));
        return Vector(x, z, y);
    }

    Int32 SpinTorusShape::get_handle_count(BaseObject* op) {
        return 2;
    }

    Bool SpinTorusShape::get_handle(BaseObject* op, Int32 handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        Vector& point = info->position;
        info->direction = Vector(1, 0, 0);
        Float radius = bc->GetFloat(PR1M_SPINTORUS_RADIUS);

        switch (handle) {
            case HANDLE_RADIUS:
                point.x = radius;
                break;
            case HANDLE_PIPERADIUS:
                point.x = radius + bc->GetFloat(PR1M_SPINTORUS_PIPERADIUS) * 2;
                break;
            default:
                return false;
        }

        return true;
    }

    void SpinTorusShape::set_handle(BaseObject* op, Int32 handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        Vector& point = info->position;

        switch (handle) {
            case HANDLE_RADIUS:
                bc->SetFloat(PR1M_SPINTORUS_RADIUS, limit_min<Float>(point.x, 0));
                break;

            case HANDLE_PIPERADIUS:
                Float radius = limit_min<Float>(bc->GetFloat(PR1M_SPINTORUS_RADIUS), 0);
                Float pradius = (point.x - radius) * 0.5;
                bc->SetFloat(PR1M_SPINTORUS_PIPERADIUS, pradius);
                break;
        }
    }

    Bool SpinTorusShape::Init(GeListNode* node) {
        if (!super::Init(node)) return false;
        BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();
        bc->SetFloat(PR1M_SPINTORUS_RADIUS, 100);
        bc->SetFloat(PR1M_SPINTORUS_PIPERADIUS, 20);
        bc->SetInt32(PR1M_COMPLEXSHAPE_USEGMENTS, 40);
        bc->SetInt32(PR1M_COMPLEXSHAPE_VSEGMENTS, 40);
        return true;
    }

    Bool register_spintorus() {
        menu::root().AddPlugin(IDS_MENU_OBJECTS, Opr1m_spintorus);
        return RegisterObjectPlugin(
            Opr1m_spintorus,
            GeLoadString(IDS_Opr1m_spintorus),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_GENERATOR,
            SpinTorusShape::alloc,
            "Opr1m_spintorus"_s,
            PR1MITIVE_ICON("Opr1m_spintorus"),
            0);
    }

} // end namespace shapes
} // end namespace pr1mitive

