/**
 * Copyright (C) 2013-2015 Niklas Rosenstein
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 * TODO: Make Effector dirty when Tag changed.
 */

#include <c4d_graphview.h>
#include <customgui_inexclude.h>
#include <NiklasRosenstein/c4d/cleanup.hpp>

#include "MoDataNode.h"
#include "XPressoEffector.h"
#include "res/description/Oxpressoeffector.h"
#include "res/c4d_symbols.h"
#include "misc/raii.h"
#include "menu.h"

#define XPRESSOEFFECTOR_VERSION  1000
#define XPRESSOEFFECTOR_PRESETNAME "xpe-preset.c4d"

// This points to the preset tag loaded from the preset file. Loaded
// once on request.
static BaseTag* g_preset_tag = nullptr;

static BaseTag* GetPresetTag() {
    if (!g_preset_tag) {
        Filename filename = GeGetPluginResourcePath() + XPRESSOEFFECTOR_PRESETNAME;
        BaseDocument* doc = LoadDocument(filename, SCENEFILTER_OBJECTS, nullptr);
        if (!doc) return nullptr;
        BaseObject* op = doc->GetFirstObject();
        g_preset_tag = (op ? op->GetTag(Texpresso) : nullptr);
        if (g_preset_tag) g_preset_tag->Remove();
        KillDocument(doc);
    }

    if (g_preset_tag) return (BaseTag*) g_preset_tag->GetClone(COPYFLAGS_0, nullptr);
    else return BaseTag::Alloc(Texpresso);
}

class XPressoEffectorData : public EffectorData {

    typedef EffectorData super;

public:

    static NodeData* Alloc() { return NewObjClear(XPressoEffectorData); }

    XPressoEffectorData() : super(), m_x_dcount(0), m_d_dcount(0) { }

    virtual ~XPressoEffectorData() { }

    //| EffectorData Overrides

    virtual void ModifyPoints(BaseObject* op, BaseObject* gen, BaseDocument* doc,
            EffectorDataStruct* data, MoData* md, BaseThread* thread) {
        if (!op || !gen || !doc || !md) return;

        // Find the MoGraph selection.
        GeData ge_selName;
        op->GetParameter(ID_MG_BASEEFFECTOR_SELECTION, ge_selName, DESCFLAGS_GET_0);
        String selName = ge_selName.GetString();
        BaseSelect* sel = nullptr;
        for (BaseTag* tag=gen->GetTag(Tmgselection); tag; tag=tag->GetNext()) {
            if (tag->IsInstanceOf(Tmgselection) && tag->GetName() == selName) {
                GetMGSelectionMessage data;
                if (tag->Message(MSG_GET_MODATASELECTION, &data)) {
                    sel = data.sel;
                }
                break;
            }
        }

        m_execInfo.md = md;
        m_execInfo.gen = gen;
        m_execInfo.falloff = GetFalloff();
        m_execInfo.sel = sel;
        if (m_execInfo.falloff && !m_execInfo.falloff->InitFalloff(nullptr, doc, op)) {
            m_execInfo.falloff = nullptr;
        }
        if (m_execInfo.falloff) m_execInfo.falloff->SetMg(op->GetMg());

        Bool easyOn = false;
        m_execInfo.easyOn = &easyOn;
        iferr (m_execInfo.easyValues = NewMemClear(Vector, md->GetCount()))
            ;
        if (!m_execInfo.easyValues) {
            GePrint(String(__FUNCTION__) + ": Failed to create easy values array.");
        }

        // Invoke all XPresso Tags.
        for (BaseTag* tag=op->GetFirstTag(); tag; tag=tag->GetNext()) {
            if (tag->IsInstanceOf(Texpresso)) {
                GvNodeMaster* master = ((XPressoTag*) tag)->GetNodeMaster();
                if (master) master->Execute(thread);
            }
        }

        if (easyOn && m_execInfo.easyValues) {
            super::ModifyPoints(op, gen, doc, data, md, thread);
        }

        if (m_execInfo.easyValues) DeleteMem(m_execInfo.easyValues);
        m_execInfo.Clean();
    }

    virtual void CalcPointValue(BaseObject* op, BaseObject* gen, BaseDocument* doc,
            EffectorDataStruct* data, Int32 index, MoData* md, const Vector& globalpos,
            Float falloff_weight) {
        if (!m_execInfo.easyValues) {
            return;
        }

        // Set everything to the calculated interpolation values.
        Vector* buf = (Vector*) data->strengths;
        for (Int32 i=0; i < BLEND_COUNT / 3; i++, buf++) {
            *buf = m_execInfo.easyValues[index];
        }
    }

    virtual Int32 GetEffectorFlags() {
        BaseObject* op = static_cast<BaseObject*>(Get());
        if (!op) return 0;

        BaseContainer* bc = op->GetDataInstance();
        if (!bc) return 0;

        Int32 flags = EFFECTORFLAGS_HASFALLOFF;
        if (bc->GetBool(XPRESSOEFFECTOR_TIMEDEPENDENT)) {
            flags |= EFFECTORFLAGS_TIMEDEPENDENT | EFFECTORFLAGS_CAMERADEPENDENT;
        }
        return flags;
    }

    //| ObjectData Overrides

    virtual EXECUTIONRESULT Execute(BaseObject* op, BaseDocument* doc, BaseThread* bt,
            Int32 priority, EXECUTIONFLAGS flags) {
        if (!_UpdateDependencies(op, doc)) return EXECUTIONRESULT_USERBREAK;
        return super::Execute(op, doc, bt, priority, flags);
    }

    //| NodeData Overrides

    virtual Bool Init(GeListNode* node) {
        if (!super::Init(node) || !node) return false;
        BaseContainer* bc = static_cast<BaseObject*>(node)->GetDataInstance();
        if (!bc) return false;
        bc->SetBool(XPRESSOEFFECTOR_TIMEDEPENDENT, false);
        bc->SetData(XPRESSOEFFECTOR_DEPENDENCIES, GeData(CUSTOMDATATYPE_INEXCLUDE_LIST, DEFAULTVALUE));
        return true;
    }

    virtual Bool Message(GeListNode* node, Int32 type, void* pData) {
        if (!node) return false;

        // Calculate the dirty-count of all XPresso tags on the object.
        Int32 dcnt = _CountXPressoTagsDirty(static_cast<BaseObject*>(node));

        // If the calculated dirty-count differs from the stored count,
        // we will mark the effector object as dirty.
        if (dcnt != m_x_dcount) {
            node->SetDirty(DIRTYFLAGS_DATA);
            m_x_dcount = dcnt;
        }

        switch (type) {
            /**
             * Sent to retrieve the *MoData* stored in the effector. This
             * is only true when called from within the `ExecuteEffector()`
             * pipeline.
             */
            case MSG_XPRESSOEFFECTOR_GETEXECINFO: {
                if (pData) {
                    *((XPressoEffector_ExecInfo*) pData) = m_execInfo;
                }
                return pData != nullptr;
            }

            /**
             * Open the XPresso Editor on a double click for the *first* XPresso
             * tag found in the objects' tag list.
             */
            case MSG_EDIT: {
                BaseTag* tag = node ? ((BaseObject*) node)->GetTag(Texpresso) : nullptr;
                GvWorld* world = GvGetWorld();
                if (tag && world) {
                    return world->OpenDialog(Texpresso, ((XPressoTag*) tag)->GetNodeMaster());
                }
                break;
            }

            /**
             * Add an XPresso Tag when the effector is inserted into the document.
             */
            case MSG_MENUPREPARE: {
                if (node) {
                    BaseTag* tag = GetPresetTag();
                    if (tag) ((BaseObject*) node)->InsertTag(tag);
                }
                break;
            }

            default:
                break;
        }
        return super::Message(node, type, pData);
    }

private:

    Bool _UpdateDependencies(BaseObject* op, BaseDocument* doc) {
        if (!op) return false;

        // This variable will hold the current dirty-count.
        Int32 dcount = 0;

        // Compute the dirty-count of the objects in the dependency
        // field.
        BaseContainer* bc = op->GetDataInstance();
        if (bc) {
            // Retrieve the InExclude list from the object.
            const InExcludeData* list = static_cast<const InExcludeData*>(
                    bc->GetCustomDataType(XPRESSOEFFECTOR_DEPENDENCIES, CUSTOMDATATYPE_INEXCLUDE_LIST)
            );

            // And compute the dirty-count of all of these.
            if (list) {
                Int32 count = list->GetObjectCount();
                for (Int32 i=0; i < count; i++) {
                    BaseList2D* obj = list->ObjectFromIndex(doc, i);
                    if (obj) {
                        dcount += obj->GetDirty(DIRTYFLAGS_DATA);
                    }
                }
            }
        }

        // Add the dirty-count of all XPresso Tags.
        dcount += _CountXPressoTagsDirty(op);

        // Compare the dirty-counts.
        if (dcount != m_d_dcount) {
            op->SetDirty(DIRTYFLAGS_DATA);
            m_d_dcount = dcount;
        }
        return true;
    }

    virtual Int32 _CountXPressoTagsDirty(BaseObject* op, DIRTYFLAGS flags=DIRTYFLAGS_ALL, HDIRTYFLAGS hflags=HDIRTYFLAGS_ALL) {
        Int32 dcnt = 0;

        // Iterate over all tags.
        for (BaseTag* tag=op->GetFirstTag(); tag; tag=tag->GetNext()) {
            // Filter XPresso Tags.
            if (tag->IsInstanceOf(Texpresso)) {
                dcnt += tag->GetDirty(flags) + tag->GetHDirty(hflags);
                GvNodeMaster* master = static_cast<XPressoTag*>(tag)->GetNodeMaster();
                if (master) dcnt += master->GetDirty(flags) + master->GetHDirty(hflags);;
            }
        }

        return dcnt;
    }

    /**
     * Contains execution information. Only valid from a call-stack level
     * below `ModifyPoints()`.
     */
    XPressoEffector_ExecInfo m_execInfo;

    /**
     * Keeps track of the dirty-count of all XPresso tags of the effector.
     */
    Int32 m_x_dcount;

    /**
     * Keeps track of the dirty-count of all dependencies.
     */
    Int32 m_d_dcount;

};

Bool RegisterXPressoEffector() {
    menu::root().AddPlugin(IDS_MENU_EFFECTORS, ID_XPRESSOEFFECTOR);
    niklasrosenstein::c4d::cleanup([] {
        BaseTag::Free(g_preset_tag);
    });
    return RegisterEffectorPlugin(
        ID_XPRESSOEFFECTOR,
        GeLoadString(IDC_XPRESSOEFFECTOR_NAME),
        OBJECT_MODIFIER | PLUGINFLAG_HIDEPLUGINMENU | OBJECT_CALL_ADDEXECUTION,
        XPressoEffectorData::Alloc,
        "Oxpressoeffector"_s,
        raii::AutoBitmap("icons/Oxpressoeffector.tif"_s),
        XPRESSOEFFECTOR_VERSION);
}
