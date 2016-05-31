// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/defines.h>
#include <pr1mitive/debug.h>
#include <pr1mitive/helpers.h>
#include <pr1mitive/activation.h>
#include <pr1mitive/shapes/BaseComplexShape.h>
#include "res/description/Opr1m_expression.h"
#include "menu.h"

namespace pr1mitive {
namespace shapes {

    class ExpressionShape : public BaseComplexShape {

        typedef BaseComplexShape super;

      public:

        static NodeData* alloc() { return gNew(ExpressionShape); }

      //
      // BaseComplexShape -------------------------------------------------------------------------
      //

        Bool init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        void free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        Bool init_thread_activity(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info, LONG thread_count);

        Vector calc_point(BaseObject* op, ComplexShapeInfo* info, Real u, Real v, LONG thread_index);

      //
      // ObjectData -------------------------------------------------------------------------------
      //

        Bool Init(GeListNode* node);

    };

    struct ExpressionData {
        AutoAlloc<Parser> parser;
        AutoAlloc<ParserCache> cache_x;
        AutoAlloc<ParserCache> cache_y;
        AutoAlloc<ParserCache> cache_z;

        // The parser cache requires the added variables to still exist
        // in memory still it is only accepting memory addresses.
        helpers::ArrayClass<Real> allocated_vars;

        Bool Init(String expr, ParserCache* dest) {
            LONG error;
            Bool result = parser->Init(expr, &error, UNIT_NONE, ANGLE_RAD);
            if (error == 0 && result) {
                parser->GetParserData(dest);
                return true;
            }
            return false;
        }

    };

    Bool ExpressionShape::init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        if (not super::init_calculation(op, bc, info)) return false;

        // NOTE: MCOMMAND_OPTIMIZE issue. Double optimization achieves correct
        // results for triangle faces merging in a single point.
        info->optimize_passes = 2;

        info->umin = bc->GetReal(PR1M_EXPRESSIONSHAPE_UMIN);
        info->umax = bc->GetReal(PR1M_EXPRESSIONSHAPE_UMAX);
        info->vmin = bc->GetReal(PR1M_EXPRESSIONSHAPE_VMIN);
        info->vmax = bc->GetReal(PR1M_EXPRESSIONSHAPE_VMAX);
        info->inverse_normals = bc->GetBool(PR1M_EXPRESSIONSHAPE_INVERSENORMALS);
        info->flip_uvw_x = bc->GetBool(PR1M_EXPRESSIONSHAPE_FLIPUVWX);
        info->flip_uvw_y = bc->GetBool(PR1M_EXPRESSIONSHAPE_FLIPUVWY);
        info->rotate_polys = bc->GetBool(PR1M_EXPRESSIONSHAPE_ROTATEPOLYGONS);

        // The data parameter will be filled in `init_thread_activity()�.
        return true;
    }

    void ExpressionShape::free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        if (info->data) {
            struct ExpressionData* data = (struct ExpressionData*) info->data;
            delete [] data;
        }
    }

    Bool ExpressionShape::init_thread_activity(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info, LONG thread_count) {
        // Allocate a parser for each thread separately.
        struct ExpressionData* data = new struct ExpressionData[thread_count];
        if (not data) return false;

        String expr_x = bc->GetString(PR1M_EXPRESSIONSHAPE_XEXPR);
        String expr_y = bc->GetString(PR1M_EXPRESSIONSHAPE_YEXPR);
        String expr_z = bc->GetString(PR1M_EXPRESSIONSHAPE_ZEXPR);

        Real tu;
        // Add the dynamic variables so the parser knows about them and
        // parsing (which will follow) can succeed.
        for (LONG i=0; i < thread_count; i++) {
            data[i].parser->AddVar("u", &tu, true);
            data[i].parser->AddVar("v", &tu, true);
        }

        // Parse the userdata and add variables respectively.
        DynamicDescription* desc = op->GetDynamicDescription();
        if (desc) {
            void* handle = desc->BrowseInit();
            const BaseContainer* itemdesc;
            DescID itemid;

            while (desc->BrowseGetNext(handle, &itemid, &itemdesc)) {
                GeData gedata;
                if (!op->GetParameter(itemid, gedata, DESCFLAGS_GET_0)) continue;

                LONG type = gedata.GetType();
                String name = itemdesc->GetString(DESC_SHORT_NAME);
                if (not name.Content() or name == "u" or name == "v") continue;

                Real value;
                Bool use_variable = true;

                switch (type) {
                    case DA_LONG:
                    case DA_REAL:
                    case DA_LLONG:
                        value = gedata.GetReal();
                        break;
                    default:
                        use_variable = false;
                        break;
                }

                if (use_variable) {
                    // Push the variable onto all parses allocated for the threads.
                    for (LONG i=0; i < thread_count; i++) {
                        // Again, the parser requires added variables to still exist in memory
                        // after adding them, this is why we keep them in this array.
                        Real* p = data[i].allocated_vars.Append(value);
                        data[i].parser->AddVar(name, p, true);
                    }
                }
            }

            desc->BrowseFree(handle);
        }

        // Initialize the parser cache for each allocated thread data and each expression.
        Bool success = true;
        for (LONG i=0; i < thread_count && success; i++) {
            success = data[i].Init(expr_x, data[i].cache_x) &&
                      data[i].Init(expr_y, data[i].cache_y) &&
                      data[i].Init(expr_z, data[i].cache_z);
        }

        info->data = data;
        return success;
    }

    Vector ExpressionShape::calc_point(BaseObject* op, ComplexShapeInfo* info, Real u, Real v, LONG thread_index) {
        struct ExpressionData* data = (struct ExpressionData*) info->data + thread_index;

		Real* pu = &u;
		Real* pv = &v;
        data->parser->AddVar("u", pu, true);
        data->parser->AddVar("v", pv, true);

        LONG error = 0;
        Vector p;
        data->parser->Calculate(data->cache_x, &p.x, &error);
        if (not error)
            data->parser->Calculate(data->cache_y, &p.y, &error);
        if (not error)
            data->parser->Calculate(data->cache_z, &p.z, &error);

        if (error) return Vector(0);
        else {
            return p;
        }
    }

    Bool ExpressionShape::Init(GeListNode* node) {
        if (not super::Init(node)) return false;
        BaseContainer* bc = ((BaseList2D*)node)->GetDataInstance();

        // bc->SetBool(PR1M_COMPLEXSHAPE_MULTITHREADING, false);
        bc->SetLong(PR1M_COMPLEXSHAPE_USEGMENTS, 10);

        bc->SetString(PR1M_EXPRESSIONSHAPE_XEXPR, "Cos(u) * 100");
        bc->SetString(PR1M_EXPRESSIONSHAPE_YEXPR, "Cos(v) * 100");
        bc->SetString(PR1M_EXPRESSIONSHAPE_ZEXPR, "Cos(u + v) * 100");

        bc->SetReal(PR1M_EXPRESSIONSHAPE_UMIN, 0);
        bc->SetReal(PR1M_EXPRESSIONSHAPE_UMAX, M_PI);
        bc->SetReal(PR1M_EXPRESSIONSHAPE_VMIN, -M_PI);
        bc->SetReal(PR1M_EXPRESSIONSHAPE_VMAX, M_PI);

        bc->SetBool(PR1M_EXPRESSIONSHAPE_INVERSENORMALS, false);
        bc->SetBool(PR1M_EXPRESSIONSHAPE_ROTATEPOLYGONS, true);
        bc->SetBool(PR1M_EXPRESSIONSHAPE_FLIPUVWX, false);
        bc->SetBool(PR1M_EXPRESSIONSHAPE_FLIPUVWY, false);
        return true;
    }

    Bool register_expression_shape() {
        menu::root().AddPlugin(IDS_MENU_OBJECTS, Opr1m_expression);
        return RegisterObjectPlugin(
            Opr1m_expression,
            GeLoadString(IDS_Opr1m_expression),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_GENERATOR,
            ExpressionShape::alloc,
            "Opr1m_expression", PR1MITIVE_ICON("Opr1m_expression"), 0);
    }

} // end namespace shapes
} // end namespace pr1mitive

