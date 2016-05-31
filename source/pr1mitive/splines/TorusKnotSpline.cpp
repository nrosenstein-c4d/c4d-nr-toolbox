// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/helpers.h>
#include <pr1mitive/activation.h>
#include <pr1mitive/splines/BaseComplexSpline.h>
#include "res/description/Opr1m_torusknot.h"
#include "menu.h"

using namespace pr1mitive::helpers;

namespace pr1mitive {
namespace splines {

    class TorusKnotSpline : public BaseComplexSpline {

        typedef BaseComplexSpline super;

      private:

        // Handle IDs
        static const char HANDLE_RADIUS = 0;

      public:

        static NodeData* alloc() { return gNew(TorusKnotSpline); }

      //
      // ComplexContour ---------------------------------------------------------------------------
      //

        Bool init_calculation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info);

        void free_caluclation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info);

        Vector calc_point(BaseObject* op, ComplexSplineInfo* info, Real u);

      //
      // BasePrimitiveData ------------------------------------------------------------------------
      //

        LONG get_handle_count(BaseObject* op);

        Bool get_handle(BaseObject* op, LONG index, HandleInfo* info);

        void set_handle(BaseObject* op, LONG index, HandleInfo* info);

      //
      // ObjectData -------------------------------------------------------------------------------
      //

        Bool Init(GeListNode* node);

    };

    struct TorusKnotData {
        Real p;
        Real q;
        Real radius;
    };

    Bool TorusKnotSpline::init_calculation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info) {
        if (not super::init_calculation(op, bc, info)) return false;
        info->min = 0;
        info->max = 2 * M_PI;

        struct TorusKnotData* data = new struct TorusKnotData;
        if (not data) return false;

        data->p = bc->GetReal(PR1M_TORUSKNOT_LEFT);
        data->q = bc->GetReal(PR1M_TORUSKNOT_RIGHT);
        data->radius = bc->GetReal(PR1M_TORUSKNOT_RADIUS) / 3.0;
        info->data = (void*) data;
        return true;
    }

    void TorusKnotSpline::free_caluclation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info) {
        struct TorusKnotData* data = (struct TorusKnotData*) info->data;
        delete data;
    }

    Vector TorusKnotSpline::calc_point(BaseObject* op, ComplexSplineInfo* info, Real u) {
        struct TorusKnotData* data = (struct TorusKnotData*) info->data;
        Real r = Cos(data->q * u) + 2;
        Real x = r * Cos(data->p * u);
        Real y = r * Sin(data->p * u);
        Real z = - Sin(data->q * u);
        return Vector(x * data->radius, y * data->radius, z * data->radius);
    }

    LONG TorusKnotSpline::get_handle_count(BaseObject* op) {
        return 1;
    }

    Bool TorusKnotSpline::get_handle(BaseObject* op, LONG index, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();

        Vector& point = info->position;
        Vector& diret = info->direction;
        switch (index) {
            case HANDLE_RADIUS:
                point.x = bc->GetReal(PR1M_TORUSKNOT_RADIUS);
                diret.x = 1;
                break;

            default:
                return false;
        }

        return true;
    }

    void TorusKnotSpline::set_handle(BaseObject* op, LONG index, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();

        Vector& point = info->position;
        switch (index) {
            case HANDLE_RADIUS:
                bc->SetReal(PR1M_TORUSKNOT_RADIUS, limit_min<Real>(point.x, 0));
                break;

            default:
                break;
        }
    }

    Bool TorusKnotSpline::Init(GeListNode* node) {
        if (not super::Init(node)) return false;
        BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();

        bc->SetReal(PR1M_TORUSKNOT_LEFT, 2);
        bc->SetReal(PR1M_TORUSKNOT_RIGHT, 3);
        bc->SetReal(PR1M_TORUSKNOT_RADIUS, 50);
        bc->SetReal(PR1M_COMPLEXSPLINE_SEGMENTS, 100);
        return true;
    }

    Bool register_torusknot() {
        menu::root().AddPlugin(IDS_MENU_OBJECTS, Opr1m_torusknot);
        return RegisterObjectPlugin(
            Opr1m_torusknot,
            GeLoadString(IDS_Opr1m_torusknot),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_ISSPLINE,
            TorusKnotSpline::alloc,
            "Opr1m_torusknot", PR1MITIVE_ICON("Opr1m_torusknot"), 0);
    }

} // end namespace splines
} // end namespace pr1mitive

