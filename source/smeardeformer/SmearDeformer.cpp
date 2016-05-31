/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * SmearDeformer.cpp
 *
 * TODO: The non-interactive mode is cluttered on animation playback + modifieng
 * the object due to multiple redraws & calculations per frame.
 */

#include "misc/legacy.h"
#include "nrUtils/Marker.h"
#include "nrUtils/Normals.h"

#include "SmearData.h"
#include "SmearHistory.h"
#include "Engines.h"

#include "res/c4d_symbols.h"
#include "res/description/Osmeardeformer.h"
#include "misc/raii.h"
#include "menu.h"

#define SMEARDEFORMER_VERSION 1000
#define S(x) String(x)

// Require Cinema 4D R14 API or newer
#if API_VERSION < 14000
    #error "Required Cinema 4D API Version is R14.000 or newer"
#endif


class SmearDeformer : public ObjectData {

    typedef ObjectData super;

public:

    SmearDeformer();

    virtual ~SmearDeformer();

    //| ObjectData overrides

    virtual Bool ModifyObject(
            BaseObject* mod, BaseDocument* doc, BaseObject* dest_,
            const Matrix& mg_dest, const Matrix& mg_mod, Real lod,
            LONG flags, BaseThread* thread);

    virtual void CheckDirty(BaseObject* op, BaseDocument* doc);

    //| NodeData overrides

    virtual Bool Init(GeListNode* node);

    virtual void Free(GeListNode* node);

    virtual Bool GetDDescription(GeListNode* node, Description* desc, DESCFLAGS_DESC& flags);

    virtual Bool GetDEnabling(
            GeListNode* node, const DescID& id, const GeData& t_data,
            DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc);

private:

    Bool EnsureEngines(BaseContainer* bc);

    Bool EnsureEngines(GeListNode* node) {
        return EnsureEngines(((BaseObject*) node)->GetDataInstance());
    }

    typedef c4d_misc::HashMap<LLONG, SmearHistory> HistoryMap;

    HistoryMap m_history;
    BaseTime m_prevtime;
    WeightingEngine* m_weighting_engine;
    SmearingEngine* m_smearing_engine;

};


SmearDeformer::SmearDeformer()
: super(), m_weighting_engine(NULL), m_smearing_engine(NULL) {
}

SmearDeformer::~SmearDeformer() {
}

Bool SmearDeformer::Init(GeListNode* node) {
    if (!node || !super::Init(node)) {
        return FALSE;
    }
    const EnginePlugin* pl_weight = GetFirstEnginePlugin(ID_WEIGHTINGENGINE);
    const EnginePlugin* pl_smear = GetFirstEnginePlugin(ID_SMEARINGENGINE);

    BaseContainer* bc = ((BaseObject*) node)->GetDataInstance();

    bc->SetBool(SMEARDEFORMER_INTERACTIVE, FALSE);
    bc->SetReal(SMEARDEFORMER_STRENGTH, 1.0);

    bc->SetLong(SMEARDEFORMER_WEIGHTINGENGINE, pl_weight ? pl_weight->id : 0);
    bc->SetBool(SMEARDEFORMER_WEIGHTINGINVERSE, FALSE);

    GeData ge_spline(CUSTOMDATATYPE_SPLINE, DEFAULTVALUE);
    SplineData* spline = (SplineData*) ge_spline.GetCustomDataType(CUSTOMDATATYPE_SPLINE);

    if (spline) spline->MakeLinearSplineLinear(2);
    bc->SetBool(SMEARDEFORMER_WEIGHTINGUSEPLINE, FALSE);
    bc->SetData(SMEARDEFORMER_WEIGHTINGSPLINE, ge_spline);

    if (spline) {
        spline->MakeLinearSplineLinear(2);
        spline->Mirror();
    }
    bc->SetBool(SMEARDEFORMER_EASEWEIGHT, TRUE);
    bc->SetData(SMEARDEFORMER_EASEWEIGHTSPLINE, ge_spline);

    bc->SetLong(SMEARDEFORMER_SMEARINGENGINE, pl_smear ? pl_smear->id : 0);
    return TRUE;
}

void SmearDeformer::Free(GeListNode* node) {
    if (m_weighting_engine) {
        BaseEngine::Free((BaseEngine*&) m_weighting_engine);
    }
    if (m_smearing_engine) {
        BaseEngine::Free((BaseEngine*&) m_smearing_engine);
    }
    super::Free(node);
}

Bool SmearDeformer::GetDDescription(GeListNode* node, Description* desc, DESCFLAGS_DESC& flags) {
    if (!node || !desc || !desc->LoadDescription(Osmeardeformer)) {
        return FALSE;
    }

    BaseContainer cycle_weight;
    BaseContainer cycle_smear;

    AutoAlloc<EnginePluginIterator> it;
    if (!it) return FALSE;

    LONG weight_id = -1;
    LONG smear_id = -1;
    for (; !it->AtEnd(); ++(*it)) {
        const EnginePlugin* pl = it->GetPtr();
        switch (pl->base) {
            case ID_WEIGHTINGENGINE:
                if (weight_id < 0) weight_id = pl->id;
                cycle_weight.SetString(pl->id, pl->name);
                break;
            case ID_SMEARINGENGINE:
                if (smear_id < 0) smear_id = pl->id;
                cycle_smear.SetString(pl->id, pl->name);
                break;
        }
    }

    EnsureEngines(node);
    if (m_weighting_engine) {
        if (!m_weighting_engine->EnhanceDescription(desc)) return FALSE;
    }
    if (m_smearing_engine) {
        if (!m_smearing_engine->EnhanceDescription(desc)) return FALSE;
    }

    AutoAlloc<AtomArray> arr;
    if (!arr) return FALSE;
    BaseContainer* item_weight = desc->GetParameterI(SMEARDEFORMER_WEIGHTINGENGINE, arr);
    BaseContainer* item_smear = desc->GetParameterI(SMEARDEFORMER_SMEARINGENGINE, arr);

    if (item_weight) item_weight->SetContainer(DESC_CYCLE, cycle_weight);
    if (item_smear) item_smear->SetContainer(DESC_CYCLE, cycle_smear);


    flags |= DESCFLAGS_DESC_LOADED;
    return TRUE;
}

Bool SmearDeformer::GetDEnabling(
        GeListNode* node, const DescID& did, const GeData& t_data,
        DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
    if (!node) return FALSE;

    LONG id = did[did.GetDepth() - 1].id;
    const BaseContainer* bc = ((BaseObject*) node)->GetDataInstance();
    switch (id) {
        case SMEARDEFORMER_WEIGHTINGSPLINE:
            return bc->GetBool(SMEARDEFORMER_WEIGHTINGUSEPLINE);
        case SMEARDEFORMER_EASEWEIGHTSPLINE:
            return bc->GetBool(SMEARDEFORMER_EASEWEIGHT);
        default:
            break;
    }

    if (id >= 5000 && id < 6000 && m_weighting_engine && bc) {
        return m_weighting_engine->GetEnabling(id, *bc, *itemdesc);
    }
    if (id >= 6000 && id < 7000 && m_smearing_engine && bc) {
        return m_smearing_engine->GetEnabling(id, *bc, *itemdesc);
    }
    return super::GetDEnabling(node, did, t_data, flags, itemdesc);
}

Bool SmearDeformer::ModifyObject(
        BaseObject* mod, BaseDocument* doc, BaseObject* dest_,
        const Matrix& mg_dest, const Matrix& mg_mod, Real lod,
        LONG flags, BaseThread* thread) {
    if (!mod || !doc || !dest_) {
        return FALSE;
    }
    if (!dest_->IsInstanceOf(Opolygon)) {
        return FALSE;
    }
    if (!nr::EnsureGeMarker(dest_)) {
        return FALSE;
    }
    if (!EnsureEngines(mod)) {
        return FALSE;
    }

    const BaseContainer* bc = mod->GetDataInstance();
    if (!bc) return FALSE;

    PolygonObject* dest = ToPoly(dest_);
    SmearData data(bc, doc, mod, dest);

    if (!m_weighting_engine->InitData(*bc, data)) {
        GePrint("Could not init data of Weighting Engine.");
        return FALSE;
    }
    if (!m_smearing_engine->InitData(*bc, data)) {
        GePrint("Could not init data of Smearing Engine.");
        return FALSE;
    }

    c4d_misc::Bool created = FALSE;
    HistoryMap::Entry* e = m_history.FindOrCreateEntry(dest->GetGUID(), created);
    if (!e) {
        GePrint(String(__FUNCTION__) + ": Could not create or find history entry.");
        return FALSE;
    }

    SmearHistory& history = e->GetValue();
    BaseTime time = doc->GetTime();
    Bool frame_changed = history.CompareTime(time);
    Bool reset = frame_changed && time == doc->GetMinTime();
    if (reset) history.Reset();

    Bool fake_session = !reset && !frame_changed && !data.interactive;

    // Obtain the polygon object's information.
    Vector* vertices = dest->GetPointW();
    LONG vertex_count = dest->GetPointCount();
    const CPolygon* faces = dest->GetPolygonR();
    LONG face_count = dest->GetPolygonCount();
    if (!vertices || !faces) {
        return FALSE;
    }

    SmearSession* session = NULL;
    LONG history_level = m_smearing_engine->GetMaxHistoryLevel();

    if (history.GetHistoryCount() <= 0) fake_session = FALSE;
    session = history.NewSession(history_level, fake_session);
    if (!session) {
        GePrint("> Could not allocate SmearSession.");
        return FALSE;
    }

    if (!session->CreateState(mg_dest, vertices, vertex_count, faces, face_count)) {
        return FALSE;
    }

    if (!m_weighting_engine->Init(data, history)) {
        GePrint("> Failed init weight");
        history.FreeSession(session);
        return FALSE;
    }
    if (!m_smearing_engine->Init(data, history)) {
        GePrint("> Failed init smear");
        history.FreeSession(session);
        return FALSE;
    }

    SReal* vxmap = dest->CalcVertexMap(mod);
    const SmearState* state = history.GetState(0);
    const Matrix i_mg_dest = !state->mg;
    LONG itercount = c4d_misc::Min<LONG>(vertex_count, state->vertex_count);

    for (LONG i=0; i < itercount; i++) {
        Real weight = m_weighting_engine->WeightVertex(i, data, history);
        if (vxmap) {
            weight *= vxmap[i];
        }
        if (data.weight_spline) {
            weight = data.weight_spline->GetPoint(weight).y;
        }
        if (data.inverted) {
            weight = 1.0 - weight;
        }
        weight *= data.strength;

        // Fill the current weight into the smear state.
        state->weights[i] = weight;

        // Accumulate the weighting
        if (data.ease_spline) {
            LONG count = history.GetHistoryCount();
            Real new_weight = weight;
            Real weight_div = 1.0;
            Real count_r = (Real) count - 1;

            for (LONG j=1; j < count; j++) {
                const SmearState* state = history.GetState(j);
                if (!state) continue;

                Real x = (Real) j / count_r;
                Real y = data.ease_spline->GetPoint(x).y;

                new_weight += state->weights[i] * y;
                weight_div += y;
            }

            weight = new_weight / weight_div;
        }

        // Smear the current vertices position.
        vertices[i] = i_mg_dest * m_smearing_engine->SmearVertex(i, weight, data, history);
    }

    m_weighting_engine->Free();
    m_smearing_engine->Free();

    if (vxmap) {
        GeFree(vxmap);
        vxmap = NULL;
    }

    if (session) {
        session->DeformationComplete(mg_dest);
        history.FreeSession(session);
    }
    return TRUE;
}

void SmearDeformer::CheckDirty(BaseObject* op, BaseDocument* doc) {
    if (!op) return;
    BaseTime time = doc->GetTime();
    BaseContainer* bc = op->GetDataInstance();
    if ((bc && bc->GetBool(SMEARDEFORMER_INTERACTIVE)) || time != m_prevtime) {
        m_prevtime = time;
        op->SetDirty(DIRTYFLAGS_DATA); // update the deformer every frame
    }
}

Bool SmearDeformer::EnsureEngines(BaseContainer* bc) {
    LONG id_weighting_engine = bc->GetBool(SMEARDEFORMER_WEIGHTINGENGINE);
    LONG id_smearing_engine = bc->GetBool(SMEARDEFORMER_SMEARINGENGINE);
    Bool reallocated = FALSE;

    BaseEngine::Realloc((BaseEngine*&) m_weighting_engine, id_weighting_engine, ID_WEIGHTINGENGINE, &reallocated);
    if (reallocated) m_weighting_engine->InitParameters(*bc);

    BaseEngine::Realloc((BaseEngine*&) m_smearing_engine, id_smearing_engine, ID_SMEARINGENGINE, &reallocated);
    if (reallocated) m_smearing_engine->InitParameters(*bc);

    return m_weighting_engine && m_smearing_engine;
}



static NodeData* AllocSmearDeformer() {
    return gNew(SmearDeformer);
}

// Extensions.cpp
extern Bool RegisterSmearExtensions();
extern Bool RegisterWeightExtensions();

Bool RegisterSmearDeformer() {
    if (GetC4DVersion() < 14000) {
        GePrint(GeLoadString(IDC_SMEARDEFORMER_VERSIONPROBLEM, "14"));
        return FALSE;
    }
    menu::root().AddPlugin(IDS_MENU_DEFORMERS, Osmeardeformer);
    RegisterDescription(Wbase, "Wbase");
    RegisterDescription(Sbase, "Sbase");
    RegisterSmearExtensions();
    RegisterWeightExtensions();
    return RegisterObjectPlugin(
            Osmeardeformer,
            GeLoadString(IDC_SMEARDEFORMER_NAME),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_MODIFIER,
            AllocSmearDeformer,
            "Osmeardeformer",
            raii::AutoBitmap("icons/Osmeardeformer.png"),
            SMEARDEFORMER_VERSION);
}




