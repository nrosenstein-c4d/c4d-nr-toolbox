// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/debug.h>
#include <pr1mitive/helpers.h>
#include <pr1mitive/activation.h>
#include <pr1mitive/shapes/BaseComplexShape.h>
#include "res/description/Opr1m_seashell.h"
#include "menu.h"

namespace pr1mitive {
namespace shapes {

    class SeashellShape : public BaseComplexShape {

        typedef BaseComplexShape super;

      private:

        // Handle IDs
        static const char HANDLE_RADIUS = 0;
        static const char HANDLE_PIPERADIUS = 1;
        static const char HANDLE_HEIGHT = 2;

      public:

        static NodeData* alloc() { return NewObjClear(SeashellShape); }

      //
      // BaseComplexShape -------------------------------------------------------------------------
      //

        Bool init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        void free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        Vector calc_point(BaseObject* op, ComplexShapeInfo* info, Float u, Float v, Int32 thread_index);

        void draw_handle_customs(BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh, Int32 handle, Int32 hitid, HandleInfo* info, Vector& mp, Vector& size, Matrix& mg, Vector& color);


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

    struct SeashellData {
        Float a; // Pipe Radius
        Float b; // Height
        Float c; // Radius
        Float n; // Convolution
        const SplineData* splr;
        const SplineData* splp;
    };

    Bool SeashellShape::init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        if (!super::init_calculation(op, bc, info)) return false;
        info->umin = 0;
        info->umax = 2;
        info->vmin = 0;
        info->vmax = 2;

        struct SeashellData* data = new struct SeashellData;
        if (!data) return false;

        data->a = bc->GetFloat(PR1M_SEASHELL_PIPERADIUS);
        data->b = bc->GetFloat(PR1M_SEASHELL_HEIGHT);
        data->c = bc->GetFloat(PR1M_SEASHELL_RADIUS);
        data->n = bc->GetFloat(PR1M_SEASHELL_DEGREES) / (2 * M_PI);
        data->splr = (const SplineData*) bc->GetCustomDataType(PR1M_SEASHELL_RADIUSSPLINE, CUSTOMDATATYPE_SPLINE);
        data->splp = (const SplineData*) bc->GetCustomDataType(PR1M_SEASHELL_PIPESPLINE, CUSTOMDATATYPE_SPLINE);

        info->data = (void*) data;
        return true;
    }

    void SeashellShape::free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        struct SeashellData* data = (struct SeashellData*) info->data;
        delete data;
    }

    Vector SeashellShape::calc_point(BaseObject* op, ComplexShapeInfo* info, Float u, Float v, Int32 thread_index) {
        struct SeashellData* data = (struct SeashellData*) info->data;
        Float x, y, z;

        // Retrieve SplineData values.
        Float t = ((v - info->vmin) / (info->vmax - info->vmin));
        Float rt, pt;

        if (data->splr) {
            rt = data->splr->GetPoint(t).y;
        }
        else {
            rt = 1.0;
        }

        if (data->splp) {
            pt = data->splp->GetPoint(t).y;
        }
        else {
            pt = 1.0;
        }

        Float h = pt;
        x = data->a * h * Cos(data->n * v * M_PI) * (1 + Cos(u * M_PI)) + data->c * rt * Cos(data->n * v * M_PI);
        y = -(data->b * 0.5 * v + data->a * h  * Sin(u * M_PI));
        z = data->a * h * Sin(data->n * v * M_PI) * (1 + Cos(u * M_PI)) + data->c * rt * Sin(data->n * v * M_PI);

        return Vector(x, y, z);
    }

    Int32 SeashellShape::get_handle_count(BaseObject* op) {
        return 3;
    }

    Bool SeashellShape::get_handle(BaseObject* op, Int32 handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();

        Vector& point = info->position;
        Vector& diret = info->direction;
        info->center = Vector(0);
        switch (handle) {
            case HANDLE_RADIUS:
                point.x = bc->GetFloat(PR1M_SEASHELL_RADIUS);
                diret.x = 1;
                break;

            case HANDLE_PIPERADIUS:
                point.x = bc->GetFloat(PR1M_SEASHELL_RADIUS) + bc->GetFloat(PR1M_SEASHELL_PIPERADIUS) * 2;
                diret.x = 1;
                break;

            case HANDLE_HEIGHT:
                point.y = -bc->GetFloat(PR1M_SEASHELL_HEIGHT);
                diret.y = 1;
                break;

            default:
                return false;
        }

        return true;
    }

    void SeashellShape::set_handle(BaseObject* op, Int32 handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();

        Vector& point = info->position;
        switch (handle) {
            case HANDLE_RADIUS:
                bc->SetFloat(PR1M_SEASHELL_RADIUS, helpers::limit_min<Float>(point.x, 0));
                break;

            case HANDLE_PIPERADIUS: {
                Float value = point.x - bc->GetFloat(PR1M_SEASHELL_RADIUS);
                value = helpers::limit_min<Float>(value * 0.5, 0);
                bc->SetFloat(PR1M_SEASHELL_PIPERADIUS, value);
                break;
            }

            case HANDLE_HEIGHT:
                bc->SetFloat(PR1M_SEASHELL_HEIGHT, helpers::limit_min<Float>(-point.y, 0));
                break;

            default: return;
        }
    }

    void SeashellShape::draw_handle_customs(BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh, Int32 handle, Int32 hitid, HandleInfo* info, Vector& mp, Vector& size, Matrix& mg, Vector& color) {
        if (handle == HANDLE_PIPERADIUS) {
            // The pipe-radius handle's line starts at the middle of it's tube.
            BaseContainer* bc = op->GetDataInstance();
            Float radius = bc->GetFloat(PR1M_SEASHELL_RADIUS);
            Float piperadius = bc->GetFloat(PR1M_SEASHELL_PIPERADIUS);
            Vector origin = Vector(radius + piperadius, 0, 0) + mp;
            bd->DrawLine(origin, info->position, 0);
        }
        else super::draw_handle_customs(op, bd, bh, handle, hitid, info, mp, size, mg, color);
    }

    Bool SeashellShape::Init(GeListNode* node) {
        if (!super::Init(node)) return false;
        BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();

        bc->SetFloat(PR1M_SEASHELL_RADIUS, 50);
        bc->SetFloat(PR1M_SEASHELL_PIPERADIUS, 50);
        bc->SetFloat(PR1M_SEASHELL_HEIGHT, 150);
        bc->SetFloat(PR1M_SEASHELL_DEGREES, 720 * M_PI / 180.0);
        bc->SetInt32(PR1M_COMPLEXSHAPE_USEGMENTS, 20);
        bc->SetInt32(PR1M_COMPLEXSHAPE_VSEGMENTS, 50);

        SplineData* spldata = SplineData::Alloc();
        GeData gespldata;

        // Initialize radius spline.
        spldata->MakePointBuffer(2);
        CustomSplineKnot* knot;
        knot = spldata->GetKnot(0);
        knot->vPos = Vector(0, 1, 0);
        knot = spldata->GetKnot(1);
        knot->vPos = Vector(1, 1, 0);
        gespldata.SetCustomDataType(CUSTOMDATATYPE_SPLINE, *spldata);
        bc->SetParameter(PR1M_SEASHELL_RADIUSSPLINE, gespldata);

        // Initialize pipe spline.
        spldata->MakeLinearSplineBezier(2);
        spldata->Flip();
        gespldata.SetCustomDataType(CUSTOMDATATYPE_SPLINE, *spldata);
        bc->SetParameter(PR1M_SEASHELL_PIPESPLINE, gespldata);

        // Free SplineData.
        SplineData::Free(spldata);
        return true;
    }

    void SeashellShape::GetDimension(BaseObject* op, Vector* mp, Vector* rad) {
        PolygonObject* cache = (PolygonObject*) op->GetCache(nullptr);
        if (cache && cache->IsInstanceOf(Opolygon)) {
            Int32 count = cache->GetPointCount();
            if (count) {
                const Vector* points = cache->GetPointR();
                MinMax mm;
                mm.Init(points[0]);
                for (int i=1; i < count; i++) {
                    mm.AddPoint(points[i]);
                }
                Vector min = mm.GetMin();
                Vector max = mm.GetMax();
                *rad = (max - min) * 0.5;
                *mp  = (min + max) * 0.5;
                return;
            }
        }
        *rad = Vector(0);
        *mp  = Vector(0);
    }

    Bool register_seashell() {
        menu::root().AddPlugin(IDS_MENU_OBJECTS, Opr1m_seashell);
        return RegisterObjectPlugin(
            Opr1m_seashell,
            GeLoadString(IDS_Opr1m_seashell),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_GENERATOR,
            SeashellShape::alloc,
            "Opr1m_seashell"_s,
            PR1MITIVE_ICON("Opr1m_seashell"),
            0);
    }

} // end namespace shapes
} // end namespace pr1mitive


