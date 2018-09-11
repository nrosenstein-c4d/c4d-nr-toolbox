// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/helpers.h>
#include <pr1mitive/activation.h>
#include <pr1mitive/shapes/BaseComplexShape.h>
#include "res/description/Opr1m_umbilictorus.h"
#include "menu.h"

namespace pr1mitive {
namespace shapes {

    class UmbilicTorusShape : public BaseComplexShape {

        typedef BaseComplexShape super;

      private:

        // Handle IDs.
        static const char HANDLE_RADIUS = 0;
        static const char HANDLE_PIPERADIUS = 1;

      public:

        static NodeData* alloc() { return NewObjClear(UmbilicTorusShape); }

      //
      // BaseComplexShape --------------------------------------------------------------------------
      //

        Bool init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        void free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        Vector calc_point(BaseObject* op, ComplexShapeInfo* info, Float u, Float v, Int32 thread_index);

      //
      // BasePrimitiveData -------------------------------------------------------------------------
      //

        Int32 get_handle_count(BaseObject* op);

        Bool get_handle(BaseObject* op, Int32 handle, HandleInfo* info);

        void set_handle(BaseObject* op, Int32 handle, HandleInfo* info);

      //
      // ObjectData --------------------------------------------------------------------------------
      //

        Bool GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags,const BaseContainer* itemdesc);

        Bool Init(GeListNode* node);

        // Computation of the Toruses dimension is way to complex.
        //void GetDimension(BaseObject* op, Vector* mp, Vector* rad);

    };

    struct UmbilicData {
        Float radius;
        Float iradius;
        Float smooth;
        Bool slice;
        Float dmin;
        Float dmax;
        Float inv_s;
    };

    Bool UmbilicTorusShape::init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        if (!super::init_calculation(op, bc, info)) return false;
        auto data = new struct UmbilicData;
        if (!data) return false;
        info->data = (void*) data;

        data->radius = bc->GetFloat(PR1M_UMBILICTORUS_RADIUS);
        data->iradius = bc->GetFloat(PR1M_UMBILICTORUS_PIPERADIUS) * 0.785;
        data->smooth = bc->GetFloat(PR1M_UMBILICTORUS_SMOOTHNESS);
        data->smooth = helpers::limit_min<Float>(data->smooth, 1.0);
        data->slice = bc->GetBool(PR1M_UMBILICTORUS_DOSLICE);
        data->dmin = bc->GetFloat(PR1M_UMBILICTORUS_DEGREEMIN);
        data->dmax = bc->GetFloat(PR1M_UMBILICTORUS_DEGREEMAX);
        data->inv_s = data->iradius / data->smooth;

        if (data->slice) {
            info->umin = data->dmin - M_PI;
            info->umax = data->dmax - M_PI;
        }
        else {
            info->umin = -M_PI;
            info->umax = M_PI;
        }

        info->vmin = -M_PI;
        info->vmax = M_PI;
        return true;
    }

    void UmbilicTorusShape::free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        auto data = (struct UmbilicData*) info->data;
        delete data;
    }

    Vector UmbilicTorusShape::calc_point(BaseObject* op, ComplexShapeInfo* info, Float u, Float v, Int32 thread_index) {
        auto data = (struct UmbilicData*) info->data;

        Float u_3 = u / 3.0;
        Float v2_ = v * 2.0;
        Float x = Sin(u) * (data->radius + data->iradius + Cos(u_3 - v2_) * data->inv_s + data->iradius * Cos(u_3 + v));
        Float y = Cos(u) * (data->radius + data->iradius + Cos(u_3 - v2_) * data->inv_s + data->iradius * Cos(u_3 + v));
        Float z = Sin(u_3 - v2_) * data->inv_s + data->iradius * Sin(u_3 + v);

        return Vector(x, -z, y);
    }

    Int32 UmbilicTorusShape::get_handle_count(BaseObject* op) {
        return 2;
    }

    Bool UmbilicTorusShape::get_handle(BaseObject* op, Int32 handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        Vector& point = info->position;
        Vector& diret = info->direction;

        Float radius = bc->GetFloat(PR1M_UMBILICTORUS_RADIUS);
        switch (handle) {
            case HANDLE_RADIUS:
                point.x = radius;
                diret.x = 1;
                break;
            case HANDLE_PIPERADIUS:
                point.x = radius + bc->GetFloat(PR1M_UMBILICTORUS_PIPERADIUS) * 2;
                diret.x = 1;
                break;

            default:
                return false;
        }
        return true;
    }

    void UmbilicTorusShape::set_handle(BaseObject* op, Int32 handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        Vector point = info->position;

        switch (handle) {
            case HANDLE_RADIUS:
                point.x = helpers::limit_min<Float>(point.x, 0);
                bc->SetFloat(PR1M_UMBILICTORUS_RADIUS, point.x);
                break;
            case HANDLE_PIPERADIUS: {
                Float radius = bc->GetFloat(PR1M_UMBILICTORUS_RADIUS);
                point.x = helpers::limit_min<Float>(point.x - radius, 0);
                bc->SetFloat(PR1M_UMBILICTORUS_PIPERADIUS, point.x * 0.5);
                break;
            }

            default:
                return;
        }
    }

    Bool UmbilicTorusShape::GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
        BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();
        switch (id[0].id) {
            case PR1M_UMBILICTORUS_DEGREEMIN:
            case PR1M_UMBILICTORUS_DEGREEMAX:
                return bc->GetBool(PR1M_UMBILICTORUS_DOSLICE);
            default:
                break;
        }
        return super::GetDEnabling(node, id, t_data, flags, itemdesc);
    }

    Bool UmbilicTorusShape::Init(GeListNode* node) {
        if (!super::Init(node)) return false;
        BaseObject* op = (BaseObject*) node;
        BaseContainer* bc = op->GetDataInstance();

        bc->SetInt32(PR1M_COMPLEXSHAPE_USEGMENTS, 40);
        bc->SetInt32(PR1M_COMPLEXSHAPE_VSEGMENTS, 21);
        bc->SetFloat(PR1M_UMBILICTORUS_RADIUS, 100);
        bc->SetFloat(PR1M_UMBILICTORUS_PIPERADIUS, 25);
        bc->SetFloat(PR1M_UMBILICTORUS_SMOOTHNESS, 2.0);
        bc->SetBool(PR1M_UMBILICTORUS_DOSLICE, false);
        bc->SetFloat(PR1M_UMBILICTORUS_DEGREEMAX, 0);
        bc->SetFloat(PR1M_UMBILICTORUS_DEGREEMAX, M_PI * 2);
        return true;
    }

    Bool register_umbilictorus() {
        menu::root().AddPlugin(IDS_MENU_OBJECTS, Opr1m_umbilictorus);
        return RegisterObjectPlugin(
            Opr1m_umbilictorus,
            GeLoadString(IDS_Opr1m_umbilictorus),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_GENERATOR,
            UmbilicTorusShape::alloc,
            "Opr1m_umbilictorus"_s,
            PR1MITIVE_ICON("Opr1m_umbilictorus"),
            0);
    }

} // end namespace shapes
} // end namespace pr1mitive

