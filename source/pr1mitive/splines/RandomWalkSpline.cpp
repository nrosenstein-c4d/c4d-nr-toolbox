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
    Int32 start;
    Int32 stop;
    Int32 seed;
    Int32 directions;
    Int32 dmin;
    Int32 dmax;
    Bool _3d;
    Bool keep_origin;
    Bool zero_angle;
    Float delta_phi;
    Int32 splinetype;
} GenData_t;

Float GetRandomAngle(GenData_t* data) {
    return static_cast<Int32>(data->directions * data->random.Get01()) * data->delta_phi * 2;
}

class RandomWalkGenerator : public ObjectData {

    typedef ObjectData super;

public:

    static NodeData* Alloc() { return NewObjClear(RandomWalkGenerator); }

    Bool Move(GenData_t* data, Vector* result, Int32 i);

    Int32 GetRandomCallCount(GenData_t* data);

    ////////// ObjectData Overrides

    SplineObject* GetContour(BaseObject* host, BaseDocument* doc, Float lod, BaseThread* bt);

    ////////// NodeData Overrides

    Bool Init(GeListNode* node);

};

Bool RandomWalkGenerator::Move(GenData_t* data, Vector* result, Int32 i) {
    Vector rot;
    Vector prev = *result;
    Vector& res = *result;

    rot.x = GetRandomAngle(data);

    if (data->_3d) {
        rot.y = GetRandomAngle(data);
        rot.z = GetRandomAngle(data);
    }

    Matrix mat = HPBToMatrix(rot, ROTATIONORDER_YXZGLOBAL);
    Int32 length = (data->dmax - data->dmin) * data->random.Get01() + data->dmin;
    res = mat * Vector(0, length, 0);
    res.Normalize();
    res *= length;

    // TODO: This implementation does not seem to be 100% correct.
    if (!data->zero_angle && (res.x || res.y || res.z) && (prev - res).GetLength() < length) {
        Float angle = std::abs(GetAngle(prev, res));
        if (angle < 0.0000001) {
            return Move(data, &prev, i);
        }
    }

    return true;
}

Int32 RandomWalkGenerator::GetRandomCallCount(GenData_t* data) {
    Int32 count = 1;
    if (data->_3d) count += 2;
    return count + 1;
}

SplineObject* RandomWalkGenerator::GetContour(BaseObject* host, BaseDocument* doc, Float lod,
                                              BaseThread* bt) {
    if (!host || !doc || !bt) return nullptr;

    BaseContainer* bc = host->GetDataInstance();
    GenData_t data;

    data.start = bc->GetInt32(RANDOMWALK_START);
    data.stop = bc->GetInt32(RANDOMWALK_STOP);
    data.directions = bc->GetInt32(RANDOMWALK_DIRECTIONS);
    data.dmin = bc->GetInt32(RANDOMWALK_STEP_MIN);
    data.dmax = bc->GetInt32(RANDOMWALK_STEP_MAX);
    data._3d = bc->GetBool(RANDOMWALK_3D);
    data.zero_angle = bc->GetBool(RANDOMWALK_ZEROANGLE);
    data.keep_origin = bc->GetBool(RANDOMWALK_KEEPORIGIN);
    data.seed = bc->GetInt32(RANDOMWALK_SEED);
    data.splinetype = bc->GetInt32(SPLINEOBJECT_TYPE);

    Int32 count = data.stop - data.start;

    if (data.start < 0) data.start = 0;
    if (data.stop < 0) data.stop = 0;
    if (data.directions < 2) data.directions = 2;
    if (data.start > data.stop) {
        swap<Int32>(data.start, data.stop);
    }

    data.delta_phi = M_PI / static_cast<Float>(data.directions);
    data.random.Init(data.seed);

    SplineObject* spline = SplineObject::Alloc(count, (SPLINETYPE) data.splinetype);
    if (!spline) return nullptr;
    BaseContainer* sbc = spline->GetDataInstance();
    if (!sbc) return nullptr;

    if (count <= 0) return spline;

    Vector pos;
    Vector temp;

    Vector* w_points = spline->GetPointW();
    if (!w_points) {
        SplineObject::Free(spline);
        return nullptr;
    }

    // Compute the first point of the spline if desired.
    if (!data.keep_origin) {
        for (Int32 i=0; i < data.start; i++) {
            Move(&data, &temp, -1);
            pos += temp;
        }
    }
    else {
        Int32 call_count = GetRandomCallCount(&data);
        for (Int32 i=0; i < data.start; i++) {
            for (Int32 j=0; j < call_count; j++) data.random.Get01();
        }
    }

    // Generate the spline points.
    for (Int32 i=0; i < count; i++) {
        w_points[i] = pos;
        Move(&data, &temp, i);
        pos += temp;
    }

    sbc->SetInt32(SPLINEOBJECT_TYPE, bc->GetInt32(SPLINEOBJECT_TYPE));
    sbc->SetBool(SPLINEOBJECT_CLOSED, bc->GetBool(SPLINEOBJECT_CLOSED));
    sbc->SetInt32(SPLINEOBJECT_INTERPOLATION, bc->GetInt32(SPLINEOBJECT_INTERPOLATION));
    sbc->SetInt32(SPLINEOBJECT_SUB, bc->GetInt32(SPLINEOBJECT_SUB));
    sbc->SetFloat(SPLINEOBJECT_ANGLE, bc->GetFloat(SPLINEOBJECT_ANGLE));
    sbc->SetFloat(SPLINEOBJECT_MAXIMUMLENGTH, bc->GetFloat(SPLINEOBJECT_MAXIMUMLENGTH));
    spline->Message(MSG_UPDATE);
    return spline;
}


Bool RandomWalkGenerator::Init(GeListNode* node) {
    if (!node || !super::Init(node)) return false;
    BaseContainer* bc = ((BaseObject*)node)->GetDataInstance();
    if (!bc) return false;
    bc->SetInt32(RANDOMWALK_START, 0);
    bc->SetInt32(RANDOMWALK_STOP, 100);
    bc->SetInt32(RANDOMWALK_DIRECTIONS, 4);
    bc->SetBool(RANDOMWALK_3D, false);
    bc->SetBool(RANDOMWALK_ZEROANGLE, false);
    bc->SetBool(RANDOMWALK_KEEPORIGIN, false);
    bc->SetInt32(RANDOMWALK_SEED, 12052);
    bc->SetFloat(RANDOMWALK_STEP_MIN, 20.0);
    bc->SetFloat(RANDOMWALK_STEP_MAX, 20.0);

    bc->SetInt32(SPLINEOBJECT_TYPE, SPLINEOBJECT_TYPE_LINEAR);
    bc->SetBool(SPLINEOBJECT_CLOSED, false);
    bc->SetInt32(SPLINEOBJECT_INTERPOLATION, SPLINEOBJECT_INTERPOLATION_ADAPTIVE);
    bc->SetInt32(SPLINEOBJECT_SUB, 8);
    bc->SetFloat(SPLINEOBJECT_ANGLE, 5 * M_PI / 180);
    bc->SetFloat(SPLINEOBJECT_MAXIMUMLENGTH, 5.0);
    return true;
}


namespace pr1mitive {
namespace splines {

Bool register_randomwalk() {
    menu::root().AddPlugin(IDS_MENU_SPLINES, Opr1m_randomwalk);
    return RegisterObjectPlugin(
        Opr1m_randomwalk,
        "Random Walk"_s,
        OBJECT_ISSPLINE | PLUGINFLAG_HIDEPLUGINMENU,
        RandomWalkGenerator::Alloc,
        "Opr1m_randomwalk"_s,
        PR1MITIVE_ICON("Opr1m_randomwalk"),
        0);
}

}} // namespace pr1mitive::splines
