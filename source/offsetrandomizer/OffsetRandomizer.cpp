/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 */

#include <c4d.h>
#include <c4d_apibridge.h>
#include <NiklasRosenstein/c4d/cleanup.hpp>
#include <NiklasRosenstein/c4d/raii.hpp>

#include "utils.h"
#include "res/description/Toffsetrandomizer.h"
#include "res/c4d_symbols.h"

#define OFFSETRANDOMIZER_VERSION 1000
#define MSG_OFFSETRANDOMIZER_SUBEXECUTION 1031176

#define ID_COMMAND_OPENCONSOLE 12305

using niklasrosenstein::c4d::auto_bitmap;

String* g_msgReachedHierarchyEnd = NULL;
String* g_msgNoTextureTagAt = NULL;

/**
 * Structure containing all transformation information for the
 * offset transformation process.
 */
struct OR_Data {
    Bool initialized;

    // IN

    Random random;

    Bool uvwOffEnabled;
    Vector uvwOffMin;
    Vector uvwOffMax;

    Bool uvwScaleEnabled;
    Vector uvwScaleMin;
    Vector uvwScaleMax;

    Bool offsetEnabled;
    Vector offsetMin;
    Vector offsetMax;

    Bool scaleEnabled;
    Vector scaleMin;
    Vector scaleMax;

    Bool rotationEnabled;
    Vector rotationMin;
    Vector rotationMax;

    Char* startexpr;
    Char* nextexpr;
    Char* execexpr;

    // OUT

    Int32 tagsAffected;

    OR_Data() {
        initialized = FALSE;
    }

    Bool Init(const BaseContainer* bc) {
        initialized = FALSE;
        if (!bc) return FALSE;

        random.Init(bc->GetInt32(OFFSETRANDOMIZER_SEED));

        uvwOffEnabled = bc->GetBool(OFFSETRANDOMIZER_UVWOFFSET_ENABLED);
        uvwOffMin.x = bc->GetFloat(OFFSETRANDOMIZER_UVWOFFSET_XMIN);
        uvwOffMax.x = bc->GetFloat(OFFSETRANDOMIZER_UVWOFFSET_XMAX);
        uvwOffMin.y = bc->GetFloat(OFFSETRANDOMIZER_UVWOFFSET_YMIN);
        uvwOffMax.y = bc->GetFloat(OFFSETRANDOMIZER_UVWOFFSET_YMAX);

        uvwScaleEnabled = bc->GetBool(OFFSETRANDOMIZER_UVWSCALE_ENABLED);
        uvwScaleMin.x = bc->GetBool(OFFSETRANDOMIZER_UVWSCALE_XMIN);
        uvwScaleMax.x = bc->GetBool(OFFSETRANDOMIZER_UVWSCALE_XMAX);
        uvwScaleMin.y = bc->GetBool(OFFSETRANDOMIZER_UVWSCALE_YMIN);
        uvwScaleMax.y = bc->GetBool(OFFSETRANDOMIZER_UVWSCALE_YMAX);

        offsetEnabled = bc->GetBool(OFFSETRANDOMIZER_OFFSET_ENABLED);
        offsetMin = bc->GetVector(OFFSETRANDOMIZER_OFFSET_MIN);
        offsetMax = bc->GetVector(OFFSETRANDOMIZER_OFFSET_MAX);

        scaleEnabled = bc->GetBool(OFFSETRANDOMIZER_SCALE_ENABLED);
        scaleMin = bc->GetVector(OFFSETRANDOMIZER_SCALE_MIN);
        scaleMax = bc->GetVector(OFFSETRANDOMIZER_SCALE_MAX);

        rotationEnabled = bc->GetBool(OFFSETRANDOMIZER_ROTATION_ENABLED);
        rotationMin = bc->GetVector(OFFSETRANDOMIZER_ROTATION_MIN);
        rotationMax = bc->GetVector(OFFSETRANDOMIZER_ROTATION_MAX);

        String _start = bc->GetString(OFFSETRANDOMIZER_STARTEXPR);
        String _next = bc->GetString(OFFSETRANDOMIZER_NEXTEXPR);
        String _exec = bc->GetString(OFFSETRANDOMIZER_EXECEXPR);
        startexpr = _start.GetCStringCopy();
        nextexpr = _next.GetCStringCopy();
        execexpr = _exec.GetCStringCopy();

        tagsAffected = 0;

        initialized = offsetEnabled || scaleEnabled || rotationEnabled
                   || uvwOffEnabled || uvwScaleEnabled;
        return initialized;
    }

    void Free() {
        if (startexpr) {
            DeleteMem(startexpr);
            startexpr = NULL;
        }
        if (nextexpr) {
            DeleteMem(nextexpr);
            nextexpr = NULL;
        }
        if (execexpr) {
            DeleteMem(execexpr);
            execexpr = NULL;
        }
    }

    Vector RandomInRange(Vector min, Vector max) {
        Float x = random.Get01();
        Float y = random.Get01();
        Float z = random.Get01();
        Vector res = (max - min);
        res.x *= x;
        res.y *= y;
        res.z *= z;
        return res + min;
    }

};

/**
 * Tag data implementation.
 */
class OffsetRandomizer : public TagData {

    typedef TagData super;

    Int32 tagsAffected;

public:

    static NodeData* Alloc() { return NewObjClear(OffsetRandomizer); }

    void UpdateBc(BaseContainer* bc=NULL);

    void RunProcessing(BaseTag* tag, BaseObject* host, Int32 mode, BaseContainer* bc, OR_Data* data);

    EXECUTIONRESULT PerformExecution(BaseTag* tag, BaseObject* host, OR_Data* data);

    //| TagData Overrides

    EXECUTIONRESULT Execute(BaseTag* tag, BaseDocument* doc, BaseObject* host,
                            BaseThread* bt, Int32 priority, EXECUTIONFLAGS flags);

    Bool AddToExecution(BaseTag* tag, PriorityList* list) {
        list->Add(tag, EXECUTIONPRIORITY_GENERATOR + 499, EXECUTIONFLAGS_0);
        return TRUE;
    }

    //| NodeData Overrides

    Bool Init(GeListNode* node);

    Bool Message(GeListNode* node, Int32 type, void* pData);

    Bool GetDEnabling(GeListNode* node, const DescID& did, const GeData& data,
                      DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc);

    Bool GetDDescription(GeListNode* node, Description* desc, DESCFLAGS_DESC& flags);

};

// ----------------------------------------------------------------------------

/**
 * Exexutes an instruction-set of DNUPC.
 */
template <typename T>
T* ExecuteInstructions(const Char* p, T* op, Bool allowNull=FALSE) {
    if (!p && !allowNull) return NULL;
    while(op && p && *p) {
        switch (*(p++)) {
            case 'd':
            case 'D':
                op = op->GetDown();
                break;
            case 'n':
            case 'N':
                op = op->GetNext();
                break;
            case 'p':
            case 'P':
                op = op->GetPred();
                break;
            case 'u':
            case 'U':
                op = op->GetUp();
                break;
            case 'c':
            case 'C':
                if (op->IsInstanceOf(Obase)) {
                    BaseObject* cop = static_cast<BaseObject*>(op);
                    op = cop->GetDeformCache();
                    if (!op) op = cop->GetCache();
                }
                break;
            default:
                p = NULL;
                break;
        }
    }
    return op;
}

void HideDescParameter(Description* desc, const DescID& parameter, Bool hidden=TRUE) {
    if (!desc) return;
    BaseContainer* item = desc->GetParameterI(parameter, NULL);
    if (!item) return;

    item->SetBool(DESC_HIDE, hidden);
}

void ModifyTag(TextureTag* tag, OR_Data& data) {
    if (!tag) return;

    BaseContainer* bc = tag->GetDataInstance();
    if (!bc) return;

    if (data.uvwOffEnabled) {
        Vector offset = data.RandomInRange(data.uvwOffMin, data.uvwOffMax);
        bc->SetFloat(TEXTURETAG_OFFSETX, offset.x);
        bc->SetFloat(TEXTURETAG_OFFSETY, offset.y);
    }

    if (data.uvwScaleEnabled) {
        Vector scale = data.RandomInRange(data.uvwScaleMin, data.uvwScaleMax);
        bc->SetFloat(TEXTURETAG_LENGTHX, scale.x);
        bc->SetFloat(TEXTURETAG_LENGTHY, scale.y);
    }

    if (data.offsetEnabled) {
        Vector offset = data.RandomInRange(data.offsetMin, data.offsetMax);
        tag->SetPos(offset);
    }

    if (data.scaleEnabled) {
        Vector scale = data.RandomInRange(data.scaleMin, data.scaleMax);
        tag->SetScale(scale);
    }

    if (data.rotationEnabled) {
        Vector rotation = data.RandomInRange(data.rotationMin, data.rotationMax);
        tag->SetRot(rotation);
    }

    tag->Message(MSG_UPDATE);
}

void OffsetRandomizer::UpdateBc(BaseContainer* bc) {
    if (!bc) bc = ((BaseTag*)Get())->GetDataInstance();
    String affected = String::IntToString(tagsAffected);
    String info = GeLoadString(IDC_OFFSETRANDOMIZER_TAGSAFFECTED, affected);
    bc->SetString(OFFSETRANDOMIZER_INFOTAGSAFFECTED, info);
}

void OffsetRandomizer::RunProcessing(BaseTag* tag, BaseObject* host, Int32 mode, BaseContainer* bc, OR_Data* data) {
    Bool modify_tags = (mode == OFFSETRANDOMIZER_MODE_NORMAL) || (mode == OFFSETRANDOMIZER_MODE_SLAVE);
    Bool execute_subs = (mode == OFFSETRANDOMIZER_MODE_MASTER);
    if (!modify_tags && !execute_subs) return;

    if (!tag || !host || !data) return;
    if (!bc) {
        bc = tag->GetDataInstance();
        if (!bc) return;
    }

    Int32 tagIndex = bc->GetInt32(OFFSETRANDOMIZER_TAGINDEX);
    if (data->initialized) {
        // Retrieve the object iteration should start from.
        BaseObject* op = ExecuteInstructions(data->startexpr, host);
        while (op) {
            // Retrieve the object the execution should be performed on.
            BaseObject* exec = ExecuteInstructions(data->execexpr, op, TRUE);
            if (exec) {
                // Find the texture-tag and execute all sub Offset
                // randomizer tags. This is necessary for nested
                // cloner objects.
                BaseTag* tag = op->GetFirstTag();
                Int32 ttindex = 0;
                while (tag) {
                    // Is it a texture tag?
                    if (modify_tags && tag->IsInstanceOf(Ttexture)) {
                        if (ttindex == tagIndex) {
                            ModifyTag(static_cast<TextureTag*>(tag), *data);
                            tagsAffected++;
                            data->tagsAffected++;
                        }
                        ttindex++;
                    }

                    // Is it an Offset Randomizer tag?
                    else if (execute_subs && tag->IsInstanceOf(Toffsetrandomizer)) {
                        tag->Message(MSG_OFFSETRANDOMIZER_SUBEXECUTION, data);
                    }

                    tag = tag->GetNext();
                }
            }

            // Retrieve the next object. If the returned object is equal to
            // the current one, we stop here because that would lead to an
            // infinite loop.
            BaseObject* next = ExecuteInstructions(data->nextexpr, op);
            if (next == op) next = NULL;
            op = next;
        }
    }
}

EXECUTIONRESULT OffsetRandomizer::PerformExecution(BaseTag* tag, BaseObject* host, OR_Data* data) {
    if (!tag || !host || !data) return EXECUTIONRESULT_OUTOFMEMORY;

    EXECUTIONRESULT result = EXECUTIONRESULT_OK;
    BaseContainer* container = tag->GetDataInstance();
    if (!data->initialized) {
        data->Init(container);
        if (!data->initialized) return result;
    }

    tagsAffected = 0;
    Int32 mode = container->GetInt32(OFFSETRANDOMIZER_MODE);
    switch (mode) {
        case OFFSETRANDOMIZER_MODE_NORMAL:
        case OFFSETRANDOMIZER_MODE_MASTER:
        case OFFSETRANDOMIZER_MODE_SLAVE:
            RunProcessing(tag, host, mode, container, data);
            break;
    }

    switch (mode) {
        case OFFSETRANDOMIZER_MODE_MASTER:
            tagsAffected = data->tagsAffected;
        case OFFSETRANDOMIZER_MODE_NORMAL:
            UpdateBc(container);
            break;
    }
    return result;
}

Bool OffsetRandomizer::Init(GeListNode* node) {
    if (!TagData::Init(node)) return FALSE;
    BaseContainer* bc = ((BaseTag*)node)->GetDataInstance();

    bc->SetInt32(OFFSETRANDOMIZER_MODE, OFFSETRANDOMIZER_MODE_NORMAL);
    bc->SetString(OFFSETRANDOMIZER_STARTEXPR, "CD"_s);
    bc->SetString(OFFSETRANDOMIZER_NEXTEXPR, "N"_s);
    bc->SetString(OFFSETRANDOMIZER_EXECEXPR, ""_s);
    bc->SetInt32(OFFSETRANDOMIZER_TAGINDEX, 0);
    bc->SetInt32(OFFSETRANDOMIZER_SEED, (long long) GeGetMilliSeconds() % 5000 + 5000);
    bc->SetString(OFFSETRANDOMIZER_INFOTAGSAFFECTED, ""_s);

    bc->SetBool(OFFSETRANDOMIZER_UVWOFFSET_ENABLED, FALSE);
    bc->SetFloat(OFFSETRANDOMIZER_UVWOFFSET_XMIN, 0);
    bc->SetFloat(OFFSETRANDOMIZER_UVWOFFSET_XMAX, 1);
    bc->SetFloat(OFFSETRANDOMIZER_UVWOFFSET_YMIN, 0);
    bc->SetFloat(OFFSETRANDOMIZER_UVWOFFSET_YMAX, 1);

    bc->SetBool(OFFSETRANDOMIZER_UVWSCALE_ENABLED, FALSE);
    bc->SetBool(OFFSETRANDOMIZER_UVWSCALE_XMIN, 1);
    bc->SetBool(OFFSETRANDOMIZER_UVWSCALE_XMAX, 1);
    bc->SetBool(OFFSETRANDOMIZER_UVWSCALE_YMIN, 1);
    bc->SetBool(OFFSETRANDOMIZER_UVWSCALE_YMAX, 1);

    bc->SetBool(OFFSETRANDOMIZER_OFFSET_ENABLED, FALSE);
    bc->SetVector(OFFSETRANDOMIZER_OFFSET_MIN, Vector(0, 0, 0));
    bc->SetVector(OFFSETRANDOMIZER_OFFSET_MAX, Vector(0, 0, 0));

    bc->SetBool(OFFSETRANDOMIZER_SCALE_ENABLED, FALSE);
    bc->SetVector(OFFSETRANDOMIZER_SCALE_MIN, Vector(100, 100, 100));
    bc->SetVector(OFFSETRANDOMIZER_SCALE_MAX, Vector(100, 100, 100));

    bc->SetBool(OFFSETRANDOMIZER_ROTATION_ENABLED, FALSE);
    bc->SetVector(OFFSETRANDOMIZER_ROTATION_MIN, Vector(0, 0, 0));
    bc->SetVector(OFFSETRANDOMIZER_ROTATION_MAX, Vector(0, 0, 0));

    tagsAffected = 0;
    UpdateBc();
    return TRUE;
}

EXECUTIONRESULT OffsetRandomizer::Execute(BaseTag* tag, BaseDocument* doc, BaseObject* host,
            BaseThread* bt, Int32 priority, EXECUTIONFLAGS flags) {
    if (!tag || !host) return EXECUTIONRESULT_OUTOFMEMORY;
    BaseContainer* bc = tag->GetDataInstance();
    if (!bc) return EXECUTIONRESULT_OUTOFMEMORY;
    Int32 mode = bc->GetInt32(OFFSETRANDOMIZER_MODE);
    if (mode == OFFSETRANDOMIZER_MODE_NORMAL || mode == OFFSETRANDOMIZER_MODE_MASTER) {
        OR_Data data;
        return PerformExecution(tag, host, &data);
    }
    return EXECUTIONRESULT_OK;
}

Bool OffsetRandomizer::Message(GeListNode* node, Int32 type, void* pData) {
    if (type == MSG_OFFSETRANDOMIZER_SUBEXECUTION) {
        BaseTag* tag = static_cast<BaseTag*>(node);
        OR_Data* data = static_cast<OR_Data*>(pData);
        if (tag && data) {
            PerformExecution(tag, tag->GetObject(), data);
            return TRUE;
        }
        return FALSE;
    }

    return super::Message(node, type, pData);
}

Bool OffsetRandomizer::GetDEnabling(GeListNode* node, const DescID& did, const GeData& data,
            DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
    Int32 id = did[did.GetDepth() - 1].id;
    BaseContainer* bc = ((BaseList2D*) node)->GetDataInstance();
    if (!bc) {
        return FALSE;
    }

    Bool enabled = bc->GetInt32(OFFSETRANDOMIZER_MODE) != OFFSETRANDOMIZER_MODE_OFF;
    if (!enabled && id > 20000 && id < 30000 && id != OFFSETRANDOMIZER_MODE) {
        return FALSE;
    }
    switch (id) {
        case OFFSETRANDOMIZER_UVWOFFSET_XMIN:
        case OFFSETRANDOMIZER_UVWOFFSET_XMAX:
        case OFFSETRANDOMIZER_UVWOFFSET_YMIN:
        case OFFSETRANDOMIZER_UVWOFFSET_YMAX:
            return bc->GetBool(OFFSETRANDOMIZER_UVWOFFSET_ENABLED);
        case OFFSETRANDOMIZER_UVWSCALE_XMIN:
        case OFFSETRANDOMIZER_UVWSCALE_XMAX:
        case OFFSETRANDOMIZER_UVWSCALE_YMIN:
        case OFFSETRANDOMIZER_UVWSCALE_YMAX:
            return bc->GetBool(OFFSETRANDOMIZER_UVWSCALE_ENABLED);
        case OFFSETRANDOMIZER_OFFSET_MIN:
        case OFFSETRANDOMIZER_OFFSET_MAX:
            return bc->GetBool(OFFSETRANDOMIZER_OFFSET_ENABLED);
        case OFFSETRANDOMIZER_SCALE_MIN:
        case OFFSETRANDOMIZER_SCALE_MAX:
            return bc->GetBool(OFFSETRANDOMIZER_SCALE_ENABLED);
        case OFFSETRANDOMIZER_ROTATION_MIN:
        case OFFSETRANDOMIZER_ROTATION_MAX:
            return bc->GetBool(OFFSETRANDOMIZER_ROTATION_ENABLED);
        default:
            break;
    }

    return super::GetDEnabling(node, did, data, flags, itemdesc);
}

Bool OffsetRandomizer::GetDDescription(GeListNode* node, Description* desc, DESCFLAGS_DESC& flags) {
    if (!node || !desc) return FALSE;
    if (!desc->LoadDescription(Toffsetrandomizer)) {
        return FALSE;
    }
    flags |= DESCFLAGS_DESC_LOADED;

    // Hide parameter-groups and seed-value in Slave mode.
    // --------------------------------------------------

    BaseContainer* bc = static_cast<BaseTag*>(node)->GetDataInstance();
    if (!bc) return TRUE;
    Int32 mode = bc->GetInt32(OFFSETRANDOMIZER_MODE);
    switch (mode) {
        case OFFSETRANDOMIZER_MODE_MASTER:
            HideDescParameter(desc, OFFSETRANDOMIZER_TAGINDEX);
            break;
        case OFFSETRANDOMIZER_MODE_SLAVE:
            HideDescParameter(desc, OFFSETRANDOMIZER_SEED);
            HideDescParameter(desc, OFFSETRANDOMIZER_UVWOFFSET_GROUP);
            HideDescParameter(desc, OFFSETRANDOMIZER_UVWSCALE_GROUP);
            HideDescParameter(desc, OFFSETRANDOMIZER_OFFSET_GROUP);
            HideDescParameter(desc, OFFSETRANDOMIZER_SCALE_GROUP);
            HideDescParameter(desc, OFFSETRANDOMIZER_ROTATION_GROUP);
            break;
    }
    if (mode != OFFSETRANDOMIZER_MODE_MASTER && mode != OFFSETRANDOMIZER_MODE_NORMAL) {
        HideDescParameter(desc, OFFSETRANDOMIZER_INFOTAGSAFFECTED);
    }

    return TRUE;
}

Bool RegisterOffsetRandomizer() {
    g_msgReachedHierarchyEnd = new String(GeLoadString(IDC_MESSAGE_REACHEDHIERARCHYEND));
    g_msgNoTextureTagAt = new String(GeLoadString(IDC_MESSAGE_NOTEXTURETAGAT));
    niklasrosenstein::c4d::cleanup([] {
        if (g_msgReachedHierarchyEnd) {
            delete g_msgReachedHierarchyEnd;
        }
        if (g_msgNoTextureTagAt) {
            delete g_msgNoTextureTagAt;
        }
    });
    return RegisterTagPlugin(
            Toffsetrandomizer,
            GeLoadString(IDC_OFFSETRANDOMIZER),
            TAG_VISIBLE | TAG_EXPRESSION | TAG_MULTIPLE,
            OffsetRandomizer::Alloc,
            "Toffsetrandomizer"_s,
            auto_bitmap("res/icons/Toffsetrandomizer.png"),
            OFFSETRANDOMIZER_VERSION);
}
