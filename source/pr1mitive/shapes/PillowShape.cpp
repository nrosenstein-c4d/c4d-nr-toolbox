// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/helpers.h>
#include <pr1mitive/activation.h>
#include <pr1mitive/shapes/BaseComplexShape.h>
#include "res/description/Opr1m_pillow.h"
#include "menu.h"

using namespace pr1mitive::helpers;

namespace pr1mitive {
namespace shapes {

    class PillowShape : public BaseComplexShape {

        typedef BaseComplexShape super;

      private:

        // Hadle IDs.
        static const char HANDLE_WIDTH = 0;
        static const char HANDLE_HEIGHT = 1;
        static const char HANDLE_DEPTH = 2;

      public:

        static NodeData* alloc() { return NewObjClear(PillowShape); }

      //
      // BaseComplexShape -------------------------------------------------------------------------
      //

        Bool init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        void free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        Vector calc_point(BaseObject* op, ComplexShapeInfo* info, Float u, Float v, Int32 thread_index);

      //
      // BasePrimitiveData ------------------------------------------------------------------------
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

    struct PillowData {
        Vector size;
    };

    Bool PillowShape::init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        if (!super::init_calculation(op, bc, info)) return false;

        info->umin = 0;
        info->umax = M_PI;
        info->vmin = -M_PI;
        info->vmax = M_PI;

        // The pillow-shape's normals are inverted by the default calculation.
        info->inverse_normals = true;
        info->flip_uvw_y = true;

        // Allocate and fill the parameters.
        struct PillowData* data = new struct PillowData;
        if (!data) return false;

        Vector temp;
        GetDimension(op, &temp, &data->size);
        info->data = (void*) data;
        return true;
    }

    void PillowShape::free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        struct PillowData* data = (struct PillowData*) info->data;
        delete data;
    }

    Vector PillowShape::calc_point(BaseObject* op, ComplexShapeInfo* info, Float u, Float v, Int32 thread_index) {
        struct PillowData* data = (struct PillowData*) info->data;
        Float x = Cos(u) * data->size.x;
        Float y = Cos(v) * data->size.y;
        Float z = Sin(u) * Sin(v) * data->size.z;
        return Vector(x, y, z);
    }

    Int32 PillowShape::get_handle_count(BaseObject* op) {
        return 3;
    }

    Bool PillowShape::get_handle(BaseObject* op, Int32 handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();

        Vector size = bc->GetVector(PR1M_PILLOW_SIZE) * 0.5;
        Vector& point = info->position;
        Vector& diret = info->direction;
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

            default:
                return false;
        };

        return true;
    }

    void PillowShape::set_handle(BaseObject* op, Int32 handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();

        Vector point = info->position * 2;
        Vector size = bc->GetVector(PR1M_PILLOW_SIZE);
        switch (handle) {
            case HANDLE_WIDTH:
                size.x = limit_min<Float>(point.x, 0);
                break;

            case HANDLE_HEIGHT:
                size.y = limit_min<Float>(point.y, 0);
                break;

            case HANDLE_DEPTH:
                size.z = limit_min<Float>(point.z, 0);
                break;

            default:
                break;
        };

        bc->SetVector(PR1M_PILLOW_SIZE, size);
    }

    Bool PillowShape::Init(GeListNode* node) {
        if (!super::Init(node)) return false;
        BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();

        bc->SetVector(PR1M_PILLOW_SIZE, Vector(200, 200, 100));
        return true;
    }

    void PillowShape::GetDimension(BaseObject* op, Vector* mp, Vector* rad) {
        BaseContainer* bc = op->GetDataInstance();
        *rad = bc->GetVector(PR1M_PILLOW_SIZE) * 0.5;
        *mp = Vector(0);
    }

    Bool register_pillow() {
        menu::root().AddPlugin(IDS_MENU_OBJECTS, Opr1m_pillow);
        return RegisterObjectPlugin(
            Opr1m_pillow,
            GeLoadString(IDS_Opr1m_pillow),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_GENERATOR,
            PillowShape::alloc,
            "Opr1m_pillow"_s,
            PR1MITIVE_ICON("Opr1m_pillow"),
            0);
    }

} // end namespace shapes
} // end namespace pr1mitive


