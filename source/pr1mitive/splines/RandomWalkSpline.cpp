/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#define _USE_MATH_DEFINES
#include <cmath>
#include <pr1mitive/defines.h>
#include <ospline.h>
#include "res/description/Opr1m_randomwalk.h"
#include "menu.h"

template <typename T> void swap(T& a, T& b) {
    T& temp = a;
    a = b;
    b = temp;
}

typedef struct GenData {
    Random random;
    LONG start;
    LONG stop;
    LONG seed;
    LONG directions;
    LONG dmin;
    LONG dmax;
    Bool _3d;
    Bool keep_origin;
    Bool zero_angle;
    Real delta_phi;
    LONG splinetype;
} GenData_t;

Real GetRandomAngle(GenData_t* data) {
    return static_cast<LONG>(data->directions * data->random.Get01()) * data->delta_phi * 2;
}

class RandomWalkGenerator : public ObjectData {

    typedef ObjectData super;

public:

    static NodeData* Alloc() { return gNew(RandomWalkGenerator); }

    Bool Move(GenData_t* data, Vector* result, LONG i);

    LONG GetRandomCallCount(GenData_t* data);

    ////////// ObjectData Overrides

    SplineObject* GetContour(BaseObject* host, BaseDocument* doc, Real lod, BaseThread* bt);

    ////////// NodeData Overrides

    Bool Init(GeListNode* node);

};

Bool RandomWalkGenerator::Move(GenData_t* data, Vector* result, LONG i) {
    Vector rot;
    Vector prev = *result;
    Vector& res = *result;

    rot.x = GetRandomAngle(data);

    if (data->_3d) {
        rot.y = GetRandomAngle(data);
        rot.z = GetRandomAngle(data);
    }

    Matrix mat = HPBToMatrix(rot, ROTATIONORDER_YXZGLOBAL);
    LONG length = (data->dmax - data->dmin) * data->random.Get01() + data->dmin;
    res = mat * Vector(0, length, 0);
    res.Normalize();
    res *= length;

    // TODO: This implementation does not seem to be 100% correct.
    if (!data->zero_angle && (res.x || res.y || res.z) && (prev - res).GetLength() < length) {
        Real angle = std::abs(VectorAngle(prev, res));
        if (angle < 0.0000001) {
            return Move(data, &prev, i);
        }
    }

    return TRUE;
}

LONG RandomWalkGenerator::GetRandomCallCount(GenData_t* data) {
    LONG count = 1;
    if (data->_3d) count += 2;
    return count + 1;
}

SplineObject* RandomWalkGenerator::GetContour(BaseObject* host, BaseDocument* doc, Real lod,
                                              BaseThread* bt) {
    if (!host || !doc || !bt) return NULL;

    BaseContainer* bc = host->GetDataInstance();
    GenData_t data;

    data.start = bc->GetLong(RANDOMWALK_START);
    data.stop = bc->GetLong(RANDOMWALK_STOP);
    data.directions = bc->GetLong(RANDOMWALK_DIRECTIONS);
    data.dmin = bc->GetLong(RANDOMWALK_STEP_MIN);
    data.dmax = bc->GetLong(RANDOMWALK_STEP_MAX);
    data._3d = bc->GetBool(RANDOMWALK_3D);
    data.zero_angle = bc->GetBool(RANDOMWALK_ZEROANGLE);
    data.keep_origin = bc->GetBool(RANDOMWALK_KEEPORIGIN);
    data.seed = bc->GetLong(RANDOMWALK_SEED);
    data.splinetype = bc->GetLong(SPLINEOBJECT_TYPE);

    LONG count = data.stop - data.start;

    if (data.start < 0) data.start = 0;
    if (data.stop < 0) data.stop = 0;
    if (data.directions < 2) data.directions = 2;
    if (data.start > data.stop) {
        swap<LONG>(data.start, data.stop);
    }

    data.delta_phi = M_PI / static_cast<Real>(data.directions);
    data.random.Init(data.seed);

    SplineObject* spline = SplineObject::Alloc(count, (SPLINETYPE) data.splinetype);
    if (!spline) return NULL;
    BaseContainer* sbc = spline->GetDataInstance();
    if (!sbc) return NULL;

    if (count <= 0) return spline;

    Vector pos;
    Vector temp;

    Vector* w_points = spline->GetPointW();
    if (!w_points) {
        SplineObject::Free(spline);
        return NULL;
    }

    // Compute the first point of the spline if desired.
    if (!data.keep_origin) {
        for (LONG i=0; i < data.start; i++) {
            Move(&data, &temp, -1);
            pos += temp;
        }
    }
    else {
        LONG call_count = GetRandomCallCount(&data);
        for (LONG i=0; i < data.start; i++) {
            for (LONG j=0; j < call_count; j++) data.random.Get01();
        }
    }

    // Generate the spline points.
    for (LONG i=0; i < count; i++) {
        w_points[i] = pos;
        Move(&data, &temp, i);
        pos += temp;
    }

    sbc->SetLong(SPLINEOBJECT_TYPE, bc->GetLong(SPLINEOBJECT_TYPE));
    sbc->SetBool(SPLINEOBJECT_CLOSED, bc->GetBool(SPLINEOBJECT_CLOSED));
    sbc->SetLong(SPLINEOBJECT_INTERPOLATION, bc->GetLong(SPLINEOBJECT_INTERPOLATION));
    sbc->SetLong(SPLINEOBJECT_SUB, bc->GetLong(SPLINEOBJECT_SUB));
    sbc->SetReal(SPLINEOBJECT_ANGLE, bc->GetReal(SPLINEOBJECT_ANGLE));
    sbc->SetReal(SPLINEOBJECT_MAXIMUMLENGTH, bc->GetReal(SPLINEOBJECT_MAXIMUMLENGTH));
    spline->Message(MSG_UPDATE);
    return spline;
}


Bool RandomWalkGenerator::Init(GeListNode* node) {
    if (!node || !super::Init(node)) return FALSE;
    BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();
    if (!bc) return FALSE;
    bc->SetLong(RANDOMWALK_START, 0);
    bc->SetLong(RANDOMWALK_STOP, 100);
    bc->SetLong(RANDOMWALK_DIRECTIONS, 4);
    bc->SetBool(RANDOMWALK_3D, FALSE);
    bc->SetBool(RANDOMWALK_ZEROANGLE, FALSE);
    bc->SetBool(RANDOMWALK_KEEPORIGIN, FALSE);
    bc->SetLong(RANDOMWALK_SEED, 12052);
    bc->SetReal(RANDOMWALK_STEP_MIN, 20.0);
    bc->SetReal(RANDOMWALK_STEP_MAX, 20.0);

    bc->SetLong(SPLINEOBJECT_TYPE, SPLINEOBJECT_TYPE_LINEAR);
    bc->SetBool(SPLINEOBJECT_CLOSED, FALSE);
    bc->SetLong(SPLINEOBJECT_INTERPOLATION, SPLINEOBJECT_INTERPOLATION_ADAPTIVE);
    bc->SetLong(SPLINEOBJECT_SUB, 8);
    bc->SetReal(SPLINEOBJECT_ANGLE, 5 * M_PI / 180);
    bc->SetReal(SPLINEOBJECT_MAXIMUMLENGTH, 5.0);
    return TRUE;
}


namespace pr1mitive {
namespace splines {

Bool register_randomwalk() {
    menu::root().AddPlugin(IDS_MENU_SPLINES, Opr1m_randomwalk);
    return RegisterObjectPlugin(Opr1m_randomwalk, "Random Walk",
            OBJECT_ISSPLINE | PLUGINFLAG_HIDEPLUGINMENU, RandomWalkGenerator::Alloc,
            "Opr1m_randomwalk", PR1MITIVE_ICON("Opr1m_randomwalk"), 0);
}

}} // namespace pr1mitive::splines
