// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/defines.h>
#include <pr1mitive/debug.h>
#include <pr1mitive/objects/BasePrimitiveData.h>
#include "res/description/Opr1m_complexshape.h"

#ifndef PR1MITIVE_SHAPES_BASECOMPLEXSHAPE_H
#define PR1MITIVE_SHAPES_BASECOMPLEXSHAPE_H

namespace pr1mitive {
namespace shapes {

    // This structure contains information that is used to generate a mesh and
    // store specific information.
    typedef struct ComplexShapeInfo {

        ComplexShapeInfo()
        : target(nullptr), useg(-1), umin(0), umax(0), vseg(-1), vmin(0), vmax(0),
          optimize(true), optimize_treshold(0.001), multithreading(true), inverse_normals(false),
          watch_for_phong(true), uvw_dest(nullptr), generate_uvw(true), flip_uvw_x(false),
          flip_uvw_y(false), rotate_polys(false), data(nullptr) {
        };

        // A pointer to the target-object that is used to store the generated
        // vertices and polygons. The object here is retuned by GetVirtualObjects().
        // One can modify or even exchange the object in ComplexShape::post_process_object().
        // This will be None on init_calculation().
        PolygonObject* target;

        // The number of u-segments and the defintion-range of the u-parameter.
        Int32 useg;
        Float umin;
        Float umax;

        // The number of v-segments and the defintion-range of the v-parameter.
        Int32 vseg;
        Float vmin;
        Float vmax;

        // true if points should be optimizied, false if not. Default is true.
        Bool optimize;
        Int32 optimize_passes;
        Float optimize_treshold;

        // true when multithreading should be done for calculating the
        // the points of the shape. When this is true, OS calls or other
        // thread un-safe operations are disallowed in
        // BaseComplexShape::calc_point(). In some cases, multithreading
        // will not be applied although this value is set to true, eg. when
        // the number of points to be computed is very low and multithreading
        // would produce overhead it can not resolve with the fact they run
        // in parallel.
        Bool multithreading;

        // true to inverse the normals on the generated mesh, false otherwise.
        // Default is false.
        Bool inverse_normals;

        // true if ComplexShape should watch out for a Phong-tag on the host-object
        // and insert it on the generated mesh, false if not. Default is true.
        Bool watch_for_phong;

        // true if a UVw map should be generated, false if not. Default is true.
        UVWTag* uvw_dest;
        Bool generate_uvw;
        Bool flip_uvw_x;
        Bool flip_uvw_y;

        // true if polygons should be rotated, false if not.
        Bool rotate_polys;

        // The space between each segment. Will be filled after init_calculation().
        Float udelta;
        Float vdelta;

        // Subclasses of ComplexShape can write a pointer to additional information
        // that is necessary to compute the meshes' vertices, etc. It has to be
        // allocated in ComplexShape::init_calculation() and freed in
        // ComplexShape::free_calculation().
        void* data;

    } ComplexShapeInfo;

    class BaseComplexShape : public objects::BasePrimitiveData {

        typedef objects::BasePrimitiveData super;

      public:

      //
      // BaseComplexShape -------------------------------------------------------------------------
      //

        // Overwriteable Methods ------------------------------------------------------------------

        // Initializes the passed ComplexShapeInfo object with the information that is necessary
        // to generate the mesh. The default implementation inserts attributes assuming that the
        // object's description is implementing the Opr1m_complexshape description.
        virtual Bool init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        // This method is called for allocating data required by threads.
        virtual Bool init_thread_activity(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info, Int32 thread_count);

        // This method is called for freeing data allocated in init_thread_activity.
        virtual void free_thread_activity(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info, Int32 thread_count);

        // Does post-processings to the object stored in *info->target*. It can even be exchanged,
        // but remember to free the old object then. Called before free_calculation().
        virtual void post_process_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        // Frees memory that was allocated in init_calculation().
        virtual void free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info);

        // This method is called for every pair of u-v parameters to generate
        // the meshes vertices.
        virtual Vector calc_point(BaseObject* op, ComplexShapeInfo* info, Float u, Float v, Int32 thread_index);

      //
      // ObjectData -------------------------------------------------------------------------------
      //

        virtual Bool Init(GeListNode* node);

        virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);

        virtual Bool Message(GeListNode* node, Int32 type, void* ptr);

    };

} // end namespace shapes
} // end namespace pr1mitive

#endif // PR1MITIVE_SHAPES_BASECOMPLEXSHAPE_H

