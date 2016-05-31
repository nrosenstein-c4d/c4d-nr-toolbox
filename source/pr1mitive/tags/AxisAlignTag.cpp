// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/defines.h>
#include <pr1mitive/alignment.h>
#include <pr1mitive/activation.h>
#include "res/description/Tpr1m_axisalign.h"
#include "menu.h"

namespace pr1mitive {
namespace tags {

    class AxisAlignTag : public TagData {

        typedef TagData super;

      public:

        static NodeData* alloc() { return gNew(AxisAlignTag); }

      //
      // TagData ----------------------------------------------------------------------------------
      //

        EXECUTIONRESULT Execute(BaseTag* tag, BaseDocument* doc, BaseObject* op, BaseThread* bt, LONG priority, EXECUTIONFLAGS flags);

        Bool AddToExecution(BaseTag* tag, PriorityList* list);

        Bool Draw(BaseTag* tag, BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh);

      //
      // NodeData ---------------------------------------------------------------------------------
      //

        Bool Init(GeListNode* node);

        Bool GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc);

    };

    EXECUTIONRESULT AxisAlignTag::Execute(BaseTag* tag, BaseDocument* doc, BaseObject* op, BaseThread* bt, LONG priority, EXECUTIONFLAGS flags) {
        if (not tag) return EXECUTIONRESULT_USERBREAK;
        if (not op) return EXECUTIONRESULT_USERBREAK;
        BaseContainer* bc = tag->GetDataInstance();
        if (not bc or not bc->GetBool(PR1M_ALIGNMENT_ENABLED)) return EXECUTIONRESULT_OK;

        // Retrieve parameters.
        Bool apply_on_cache = bc->GetBool(PR1M_ALIGNMENT_USECACHE);
        Bool include_hierarchy = bc->GetBool(PR1M_ALIGNMENT_INCLUDEHIERARCHY);

        // Matrix mg = op->GetMg();
        Matrix up_mg;
        if (apply_on_cache) {
            BaseObject* cache = op->GetDeformCache();
            if (!cache) cache = op->GetCache();
            if (!cache) return EXECUTIONRESULT_OK;
            up_mg = op->GetMg();
            op = cache;
        }
        else {
            up_mg = op->GetUpMg();
        }

        // Compute the bounding-box.
        alignment::AABBHelper bb(!up_mg);
        bb.expand(op, include_hierarchy);

        // Compute the bounding-box size and mid-point.
        Matrix ml = op->GetMl();
        Vector mp = bb.midpoint() - ml.off;
        Vector size = bb.size();
        Vector delta = up_mg.off - mp;

        // Compute the offset specified in the tags' parameters.
        LONG mul = (bc->GetBool(PR1M_ALIGNMENT_INVERSEOFF) ? -1 : 1);
        Vector offmul = bc->GetVector(PR1M_ALIGNMENT_OFFMUL);
        Vector offset = bc->GetVector(PR1M_ALIGNMENT_OFF) * mul * offmul;

        // Align the object to the axises.
        Vector info;
        info.x = bc->GetReal(PR1M_ALIGNMENT_X);
        info.y = bc->GetReal(PR1M_ALIGNMENT_Y);
        info.z = bc->GetReal(PR1M_ALIGNMENT_Z);
        ml.off = mp + (size ^ info) + offset + delta * 2 - up_mg.off * 2;
        if (op) op->SetMl(ml);

        BaseObject* dest = (BaseObject*) bc->GetLink(PR1M_ALIGNMENT_INDICATOR, doc);
        if (dest) {
            Matrix nmg;
            nmg.off += mp;
            dest->SetMg(nmg);
            dest->SetAbsScale(size);
            dest->Message(MSG_UPDATE);
        }
        return EXECUTIONRESULT_OK;
    }

    Bool AxisAlignTag::AddToExecution(BaseTag* tag, PriorityList* list) {
        BaseContainer* bc = tag->GetDataInstance();

        // If "Apply on Cache" is checked, we want to execute the tag after
        // the host object has been generated.
        if (bc && bc->GetBool(PR1M_ALIGNMENT_USECACHE)) {
            list->Add(tag, EXECUTIONPRIORITY_GENERATOR, EXECUTIONFLAGS_CACHEBUILDING | EXECUTIONFLAGS_EXPRESSION);
            return true;
        }

        // Use the default behavior (which is reading out the priority value
        // from the tags attributes).
        return false;
    }

    Bool AxisAlignTag::Draw(BaseTag* tag, BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh) {
        return true;
    }

    Bool AxisAlignTag::Init(GeListNode* node) {
        if (not node) return false;
        BaseTag* tag = (BaseTag*) node;
        BaseContainer* bc = tag->GetDataInstance();
        if (not bc) return false;

        bc->SetBool(PR1M_ALIGNMENT_ENABLED, true);

        // The default-values is always the center.
        bc->SetReal(PR1M_ALIGNMENT_X, 0.0);
        bc->SetReal(PR1M_ALIGNMENT_Y, 0.0);
        bc->SetReal(PR1M_ALIGNMENT_Z, 0.0);

        bc->SetVector(PR1M_ALIGNMENT_OFF, Vector(0));
        bc->SetVector(PR1M_ALIGNMENT_OFFMUL, Vector(1));
        bc->SetBool(PR1M_ALIGNMENT_INVERSEOFF, false);

        bc->SetBool(PR1M_ALIGNMENT_USECACHE, false);
        bc->SetBool(PR1M_ALIGNMENT_INCLUDEHIERARCHY, false);
        bc->SetLink(PR1M_ALIGNMENT_INDICATOR, NULL);
        return true;
    }

    Bool AxisAlignTag::GetDEnabling(GeListNode* node, const DescID& id, const GeData& t_data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
        LONG rid = id[0].id;
        BaseContainer* bc = ((BaseTag*) node)->GetDataInstance();
        switch (rid) {
            // The priority parameter should only be editable when "Apply on Cache"
            // is not checked.
            case EXPRESSION_PRIORITY:
                return (bc ? not bc->GetBool(PR1M_ALIGNMENT_USECACHE) : false);
        }

        return super::GetDEnabling(node, id, t_data, flags, itemdesc);
    }

    Bool register_axisalign_tag() {
        menu::root().AddPlugin(Tpr1m_axisalign);
        return RegisterTagPlugin(
            Tpr1m_axisalign,
            GeLoadString(IDS_Tpr1m_axisalign),
            TAG_VISIBLE | TAG_EXPRESSION,
            AxisAlignTag::alloc,
            "Tpr1m_axisalign", PR1MITIVE_ICON("Tpr1m_axisalign"), 0);
    };

} // end namespace tags
} // end namespace pr1mitive

