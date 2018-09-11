// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/helpers.h>
#include <pr1mitive/activation.h>
#include <pr1mitive/shapes/BaseComplexShape.h>
#include "res/description/Opr1m_jet.h"
#include "menu.h"

namespace pr1mitive {
namespace shapes {

    class JetShape : public BaseComplexShape {

        typedef BaseComplexShape super;

      private:

        // Handle IDs.
        static const char HANDLE_WIDTH = 0;
        static const char HANDLE_HEIGHT = 1;
        static const char HANDLE_DEPTH = 2;

      public:

        static NodeData* alloc() { return NewObjClear(JetShape); }

      //
      // BaseComplexShape -------------------------------------------------------------------------
      //

        Bool init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        void free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        Vector calc_point(BaseObject* op, ComplexShapeInfo* info, Float u, Float v, Int32 thread_index);

      //
      // BasePrimitveData -------------------------------------------------------------------------
      //

        Int32 get_handle_count(BaseObject* op);

        Bool get_handle(BaseObject* op, Int32 handle, HandleInfo* info);

        void set_handle(BaseObject* op, Int32 handle, HandleInfo* info);

      //
      // ObjectData -------------------------------------------------------------------------------
      //

        Bool Init(GeListNode* node);

        void GetDimension(BaseObject* op, Vector* mp, Vector* rad);

    };

    struct JetData {
        Vector size;
    };

    Bool JetShape::init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        if (!super::init_calculation(op, bc, info)) return false;

        // NOTE: MCOMMAND_OPTIMIZE issue. Double optimization achieves correct
        // results for triangle faces merging in a single point.
        info->optimize_passes = 2;

        info->umin = 0;
        info->umax = M_PI;
        info->vmin = 0;
        info->vmax = 2 * M_PI;

        struct JetData* data = new struct JetData;
        if (!data) return false;

        data->size = bc->GetVector(PR1M_JET_SIZE);
        data->size.x /= 3;
        data->size.y /= 3;
        info->data = (void*) data;
        return true;
    }

    void JetShape::free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        auto data = (struct JetData*) info->data;
        delete data;
    }

    Vector JetShape::calc_point(BaseObject* op, ComplexShapeInfo* info, Float u, Float v, Int32 thread_index) {
        // Maximum value of cosh() is cosh(pi) in this case.
        static const Float u_max = cosh(M_PI);
        static const Float u_min = 1.0;

        // Retrieve the information that was created in init_calculation() and compute the
        // point at the passed parameters.
        auto data = (struct JetData*) info->data;
        Float x = (1 - cosh(u)) * Sin(u) * Cos(v) * 0.5 * data->size.x;
        Float y = (1 - cosh(u)) * Sin(u) * Sin(v) * 0.5 * data->size.y;
        Float z = ((cosh(u) - u_min) / (u_max - u_min)) * data->size.z - (data->size.z * 0.5);

        return Vector(x, y, z);
    }

    Int32 JetShape::get_handle_count(BaseObject* op) {
        return 3;
    }

    Bool JetShape::get_handle(BaseObject* op, Int32 handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        Vector& point = info->position;
        Vector& diret = info->direction;
        Vector size = bc->GetVector(PR1M_JET_SIZE);

        switch (handle) {
            case HANDLE_WIDTH:
                point.x = size.x;
                diret.x = 1;
                break;

            case HANDLE_HEIGHT:
                point.y = size.y;
                diret.y = 1;
                break;

            case HANDLE_DEPTH:
                point.z = size.z;
                diret.z = 1;
                break;
        }

        point *= 0.5;
        return true;
    }

    void JetShape::set_handle(BaseObject* op, Int32 handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        Vector point = info->position * 2;
        Vector size  = bc->GetVector(PR1M_JET_SIZE);

        switch (handle) {
            case HANDLE_WIDTH:
                size.x = helpers::limit_min<Float>(point.x, 0);
                break;

            case HANDLE_HEIGHT:
                size.y = helpers::limit_min<Float>(point.y, 0);
                break;

            case HANDLE_DEPTH:
                size.z = helpers::limit_min<Float>(point.z , 0);
                break;
        }

        bc->SetVector(PR1M_JET_SIZE, size);
    }

    Bool JetShape::Init(GeListNode* node) {
        if (!super::Init(node)) return false;
        BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();
        bc->SetVector(PR1M_JET_SIZE, Vector(50, 50, 200));
        return true;
    }

    void JetShape::GetDimension(BaseObject* op, Vector* mp, Vector* rad) {
        BaseContainer* bc = op->GetDataInstance();
        *rad = bc->GetVector(PR1M_JET_SIZE) * 0.5;
        *mp = Vector(0);
    }

    Bool register_jet_shape() {
        menu::root().AddPlugin(IDS_MENU_OBJECTS, Opr1m_jet);
        return RegisterObjectPlugin(
            Opr1m_jet,
            GeLoadString(IDS_Opr1m_jet),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_GENERATOR,
            JetShape::alloc,
            "Opr1m_jet"_s,
            PR1MITIVE_ICON("Opr1m_jet"),
            0);
    }

} // end namespace shapes
} // end namespace pr1mitive


