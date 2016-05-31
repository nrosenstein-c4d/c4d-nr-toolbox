/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * Extensions.cpp
 */

#define _USE_MATH_DEFINES
#include <cmath>

#include "Engines.h"

#include "res/c4d_symbols.h"
#include "res/description/Osmeardeformer.h"

#include "res/description/Spositional.h"
#include "res/description/Shistorical.h"
#include "res/description/Wvertexnormal.h"

class PositionalSmearing : public SmearingEngine {

public:

    static BaseEngine* Alloc() { return gNew(PositionalSmearing); }

    //| SmearingEngine overrides

    virtual LONG GetMaxHistoryLevel() const {
        return 1; // We only need the previous position
    }

    virtual Vector SmearVertex(LONG vertex_index, Real weight, const SmearData& data, const SmearHistory& history) {
        const SmearState* prev = history.GetState(1);
        const SmearState* curr = history.GetState(0);
        if (!curr) return Vector();

        // We can garuantee that the vertex_index will not exceed the
        // bounds of the current smear state.
        const Vector& curr_p = curr->original_vertices[vertex_index];

        // But we can not garuantee this for the previous state.
        if (!prev || vertex_index >= prev->vertex_count) {
            return curr_p;
        }

        const Vector& prev_p = prev->original_vertices[vertex_index];
        return curr_p * (1.0 - weight) + prev_p * weight;
    }

};

class HistoricalSmearing : public SmearingEngine {

public:

    static BaseEngine* Alloc() { return gNew(HistoricalSmearing); }

    HistoricalSmearing() : SmearingEngine(), m_count(0) { }

    //| SmearingEngine overrides

    virtual LONG GetMaxHistoryLevel() const {
        return m_count;
    }

    virtual Vector SmearVertex(LONG vertex_index, Real weight, const SmearData& data, const SmearHistory& history) {
        if (weight < 0.0) weight = 0.0;
        if (weight > 1.0) weight = 1.0;

        LONG count = history.GetHistoryCount();
        count = c4d_misc::Min<LONG>(count, m_count);
        if (count <= 0) return Vector();

        count -= 1;
        LONG frame_1 = (count) * weight;
        LONG frame_2 = frame_1 + 1;

        const SmearState* state_1 = history.GetState(frame_1);
        const SmearState* state_2 = history.GetState(frame_2);
        if (!state_1 || vertex_index >= state_1->vertex_count) {
            // TODO: This shouldn't happen!!
            return Vector();
        }

        const Vector& p1 = state_1->original_vertices[vertex_index];
        if (!state_2 || vertex_index >= state_2->vertex_count) {
            return p1;
        }
        const Vector& p2 = state_2->original_vertices[vertex_index];

        Real rweight = fmod(count * weight, 1.0);
        return p1 * (1.0 - rweight) + p2 * rweight;
    }

    //| BaseEngine overrides

    virtual Bool InitParameters(BaseContainer& bc) {
        if (bc.GetType(SHISTORICAL_TIMEDELTA) != DA_REAL) {
            bc.SetReal(SHISTORICAL_TIMEDELTA, 0.1);
        }
        return TRUE;
    }

    virtual Bool InitData(const BaseContainer& bc, const SmearData& data) {
        m_count = data.doc->GetFps() * bc.GetReal(SHISTORICAL_TIMEDELTA);
        return TRUE;
    }

private:

    LONG m_count;

};


class VertexNormalWeighting : public WeightingEngine {

public:

    static BaseEngine* Alloc() { return gNew(VertexNormalWeighting); }

    VertexNormalWeighting() : WeightingEngine(), m_vertex_based(FALSE) { }

    Vector GetVertexMovement(LONG index, const SmearState* curr, const SmearState* prev) {
        if (m_vertex_based) {
            if (index >= curr->vertex_count || index >= prev->vertex_count) return Vector();
            return curr->original_vertices[index] - prev->original_vertices[index];
        }
        else {
            return curr->mg.off - prev->mg.off;
        }
    }

    //| WeightingEngine overrides

    virtual Real WeightVertex(LONG vertex_index, const SmearData& data, const SmearHistory& history) {
        const SmearState* curr = history.GetState(0);
        const SmearState* prev = history.GetState(1);
        if (!curr || !prev) return 0.0;
        if (vertex_index >= prev->vertex_count) return 0.0;

        static const Real ellipsis = 0.0000001;

        // Figure the movement direction. If there was not movement in the last frames,
        // we will check the previous frames until a movement was found.
        Vector mvdir = GetVertexMovement(vertex_index, curr, prev);
        LONG state_index = 2;
        while (mvdir.GetSquaredLength() <= ellipsis && state_index < history.GetHistoryCount()) {
            const SmearState* curr = history.GetState(state_index - 1);
            const SmearState* prev = history.GetState(state_index);
            mvdir = GetVertexMovement(vertex_index, curr, prev);
            state_index++;
        }

        // No movement figured, no weighting.
        if (mvdir.GetSquaredLength() < ellipsis) {
            return 0.0;
        }

        // Again, we _can_ assume that the vertex_index will not exceed
        // the bounds of the _current_ state.
        const Vector& vxdir = curr->original_normals[vertex_index];

        Real angle = VectorAngle(mvdir, vxdir);
        return angle / M_PI;
    }

    //| BaseEngine overrides

    virtual Bool InitParameters(BaseContainer& bc) {
        if (bc.GetType(WVERTEXNORMAL_VERTEXBASEDMOVEMENT) != DA_LONG) {
            bc.SetBool(WVERTEXNORMAL_VERTEXBASEDMOVEMENT, TRUE);
        }
        return TRUE;
    }

    virtual Bool InitData(const BaseContainer& bc, const SmearData& data) {
        m_vertex_based = bc.GetBool(WVERTEXNORMAL_VERTEXBASEDMOVEMENT);
        return TRUE;
    }

private:

    Bool m_vertex_based;

};


Bool RegisterSmearExtensions() {
    RegisterEnginePlugin(
            Wvertexnormal,
            ID_WEIGHTINGENGINE,
            GeLoadString(IDC_WEIGHTING_VERTEXNORMAL),
            "Wvertexnormal",
            VertexNormalWeighting::Alloc);
    RegisterEnginePlugin(
            Spositional,
            ID_SMEARINGENGINE,
            GeLoadString(IDC_SMEARING_POSITIONAL),
            "Spositional",
            PositionalSmearing::Alloc);
    RegisterEnginePlugin(
            Shistorical,
            ID_SMEARINGENGINE,
            GeLoadString(IDC_SMEARING_HISTORICAL),
            "Shistorical",
            HistoricalSmearing::Alloc);
    return TRUE;
}

Bool RegisterWeightExtensions() {
    return TRUE;
}

