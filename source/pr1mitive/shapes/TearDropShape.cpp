// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/helpers.h>
#include <pr1mitive/activation.h>
#include <pr1mitive/shapes/BaseComplexShape.h>
#include "res/description/Opr1m_teardrop.h"
#include "menu.h"

using namespace pr1mitive::helpers;

namespace pr1mitive {
namespace shapes {

    class TearDropShape : public BaseComplexShape {

        typedef BaseComplexShape super;

      private:

        // Handle IDs
        static const char HANDLE_LENGTH = 0;

      public:

        static NodeData* alloc() { return gNew(TearDropShape); }

      //
      // BaseComplexShape --------------------------------------------------------------------------
      //

        Bool init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        void free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

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

    struct TearDropData {
        Real a; // A
        Real b; // B
        Real z; // Z Length (or Spherification radius)
        Real s; // Spherify anmount
    };

    Bool TearDropShape::init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        if (not super::init_calculation(op, bc, info)) return false;
        info->umin = 0;
        info->umax = M_PI;
        info->vmin = 0;
        info->vmax = 2 * M_PI;

        auto data = new struct TearDropData;
        if (not data) return false;
        info->data = (void*) data;
        data->a = bc->GetReal(PR1M_TEARDROP_A);
        data->b = bc->GetReal(PR1M_TEARDROP_B);
        data->z = bc->GetReal(PR1M_TEARDROP_LENGTH) * 0.5;
        data->s = bc->GetReal(PR1M_TEARDROP_SPHERIFY);
        return true;
    }

    void TearDropShape::free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        auto data = (struct TearDropData*) info->data;
        delete data;
    }

    Vector TearDropShape::calc_point(BaseObject* op, ComplexShapeInfo* info, Real u, Real v, LONG thread_index) {
        auto data = (struct TearDropData*) info->data;
        Real x = data->a * (data->b - Cos(u)) * Sin(u) * Cos(v);
        Real z = data->a * (data->b - Cos(u)) * Sin(u) * Sin(v);
        Real y = Cos(u) * data->z;
        Vector p(x, y, z);
        return data->s * (p.GetNormalized() * data->z) + (1.0 - data->s) * p;
    }

    LONG TearDropShape::get_handle_count(BaseObject* op) {
        return 1;
    }

    Bool TearDropShape::get_handle(BaseObject* op, LONG handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        Vector& point = info->position;
        Vector& diret = info->direction;
        switch (handle) {
            case HANDLE_LENGTH:
                point.y = bc->GetReal(PR1M_TEARDROP_LENGTH) * 0.5;
                diret.y = 1;
                break;
            default:
                return false;
        }
        return true;
    }

    void TearDropShape::set_handle(BaseObject* op, LONG handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        Vector& point = info->position;
        switch (handle) {
            case HANDLE_LENGTH:
                bc->SetReal(PR1M_TEARDROP_LENGTH, limit_min<Real>(point.y, 0) * 2);
                break;
        }
    }

    Bool TearDropShape::Init(GeListNode* node) {
        if (not super::Init(node)) return false;
        BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();
        bc->SetReal(PR1M_TEARDROP_A, 50);
        bc->SetReal(PR1M_TEARDROP_B, 1);
        bc->SetReal(PR1M_TEARDROP_LENGTH, 200);
        return true;
    }

    Bool register_teardrop() {
        menu::root().AddPlugin(IDS_MENU_OBJECTS, Opr1m_teardrop);
        return RegisterObjectPlugin(
            Opr1m_teardrop,
            GeLoadString(IDS_Opr1m_teardrop),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_GENERATOR,
            TearDropShape::alloc,
            "Opr1m_teardrop",
            PR1MITIVE_ICON("Opr1m_teardrop"), 0);
    }

} // end namespace shapes
} // end namespace pr1mitive

