/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * SmearData.cpp
 */

#include "SmearData.h"

#include "res/description/Osmeardeformer.h"

SmearData::SmearData(const BaseContainer* bc, BaseDocument* doc, BaseObject* mod, PolygonObject* dest)
: doc(doc), mod(mod), dest(dest) {
    interactive = bc->GetBool(SMEARDEFORMER_INTERACTIVE);
    strength = bc->GetReal(SMEARDEFORMER_STRENGTH);
    inverted = bc->GetBool(SMEARDEFORMER_WEIGHTINGINVERSE);

    weight_spline = NULL;
    if (bc->GetBool(SMEARDEFORMER_WEIGHTINGUSEPLINE)) {
        weight_spline = (const SplineData*) bc->GetCustomDataType(
                SMEARDEFORMER_WEIGHTINGSPLINE, CUSTOMDATATYPE_SPLINE);
    }

    ease_spline = NULL;
    if (bc->GetBool(SMEARDEFORMER_EASEWEIGHT)) {
        ease_spline = (const SplineData*) bc->GetCustomDataType(
                SMEARDEFORMER_EASEWEIGHTSPLINE, CUSTOMDATATYPE_SPLINE);
    }
}

