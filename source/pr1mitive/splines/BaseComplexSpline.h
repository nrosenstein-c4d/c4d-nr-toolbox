// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/defines.h>
#include <pr1mitive/objects/BasePrimitiveData.h>
#include "res/description/Opr1m_complexspline.h"

#ifndef PR1MITIVE_SPLINES_BASECOMPLEXSPLINE_H
#define PR1MITIVE_SPLINES_BASECOMPLEXSPLINE_H

namespace pr1mitive {
namespace splines {

    // This structure contains the information that is necessary to generate a spline.
    typedef struct ComplexSplineInfo {

        ComplexSplineInfo()
        : target(null), seg(-1), min(0), max(0), type(SPLINETYPE_BSPLINE), closed(true),
        optimize(false), optimize_treshold(0.01), delta(0.0), data(null) {
        };

        //  The target-object. This is null on BaseComplexSpline::init_calculation().
        SplineObject* target;

        // The number of u-segments and the defintion-range of the u-parameter.
        LONG seg;
        Real min;
        Real max;

        // The spline-type to allocated. Default is SPLINETYPE_BSPLINE.
        SPLINETYPE type;

        // True if the spline should be closed, False if not. Default is True.
        Bool closed;

        // True if points should be optimizied, False if not. Default is False.
        Bool optimize;
        Real optimize_treshold;

        // The space between each segment. Will be filled after init_calculation().
        Real delta;

        // Subclasses of ComplexContour can write a pointer to additional information
        // that is necessary to compute the meshes' vertices, etc. It has to be
        // allocated in ComplexContour::init_calculation() and freed in
        // ComplexContour::free_calculation().
        void* data;

    } ComplexSplineInfo;

    class BaseComplexSpline : public objects::BasePrimitiveData {

        typedef objects::BasePrimitiveData super;

      public:

      //
      // BaseComplexSpline ------------------------------------------------------------------------
      //

        // Overwriteable Methods ------------------------------------------------------------------

        // Initializes the passed ComplexSplineInfo object with the information that is necessary
        // to generate the spline. The default implementation inserts attributes assuming that the
        // object's description is implementing the Opr1m_complexspline description.
        virtual Bool init_calculation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info);

        // Does post-processings to the object stored in *info->target*. It can even be exchanged,
        // but remember to free the old object then. Called before free_calculation().
        // The default implementation copys the parameters from the Osplineprimitive description
        // to the created spline-object.
        virtual void post_process_calculation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info);

        // Frees memory that was allocated in init_calculation().
        virtual void free_calculation(BaseObject* op, BaseContainer* bc, ComplexSplineInfo* info);

        // This method is called to obtain a point in the spline for the passed
        // u-parameter.
        virtual Vector calc_point(BaseObject* op, ComplexSplineInfo* info, Real u);

      //
      // ObjectData -------------------------------------------------------------------------------
      //

        virtual Bool Init(GeListNode* node);

        virtual SplineObject* GetContour(BaseObject* op, BaseDocument* doc, Real lod, BaseThread* bt);

        virtual Bool GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc);

    };

} // end namespace splines
} // end namespace pr1mitive

#endif // PR1MITIVE_SPLINES_BASECOMPLEXSPLINE_H

