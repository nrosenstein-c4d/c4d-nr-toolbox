/**
 * Copyright (C) 2013, Niklas Rosenstein
 * All rights reserved.
 *
 * ProxyGenerator.cpp
 *
 * TODO:
 *
 * - Global matrix of virtual objects is not built correctly.
 *   The `PR1M_PROXYGEN_VLEVEL` option is disabled for now.
 */

#define PROXYGEN_VERSION 1000
#define ID_PROXYGEN_PRIVATE_ISTOUCHED 1030931

#include <c4d.h>
#include <customgui_inexclude.h>
#include <pr1mitive/defines.h>
#include <pr1mitive/helpers.h>
#include <pr1mitive/alignment.h>
#include <pr1mitive/activation.h>
#include "res/description/Opr1m_proxygen.h"
#include "menu.h"

namespace pr1mitive {
namespace objects {

    struct ProxyTuple {
        BaseObject* op;
        BaseTag* tex;

        ProxyTuple() : op(nullptr), tex(nullptr) { }

        ProxyTuple(BaseObject* op, BaseTag* tex) : op(op), tex(tex) { }
    };

    static BaseTag* find_texture(BaseObject* op, Bool searchParents=true) {
        do {
            BaseTag* tag = op->GetFirstTag();
            BaseTag* lastTag = nullptr;
            while (tag) {
                if (tag->GetType() == Ttexture)
                    lastTag = tag;
                tag = tag->GetNext();
            }

            if (lastTag)
                return lastTag;

            op = op->GetUp();
        } while (searchParents && op);
        return nullptr;
    }

    static void make_proxy(
                PolygonObject* op, const Matrix& mgi, Vector* points,
                CPolygon* polys, Int32 offPoints, Int32 offPolys,
                ProxyTuple& tuple) {

        BaseObject* src = tuple.op;
        BaseTag* tex = tuple.tex;

        const Vector size = src->GetRad();
        const Vector off = src->GetMp();
        const Matrix pmg = src->GetMg();
        Matrix mg = pmg * mgi;

        points += offPoints;
        points[0] = Vector(-size.x, -size.y, -size.z);
        points[1] = Vector(-size.x, +size.y, -size.z);
        points[2] = Vector(+size.x, -size.y, -size.z);
        points[3] = Vector(+size.x, +size.y, -size.z);
        points[4] = Vector(+size.x, -size.y, +size.z);
        points[5] = Vector(+size.x, +size.y, +size.z);
        points[6] = Vector(-size.x, -size.y, +size.z);
        points[7] = Vector(-size.x, +size.y, +size.z);

        for (Int32 i=0; i < 8; i++) {
            points[i] = mg * (off + points[i]);
        }

        polys += offPolys;
        polys[0] = CPolygon(0, 1, 3, 2);
        polys[1] = CPolygon(2, 3, 5, 4);
        polys[2] = CPolygon(4, 5, 7, 6);
        polys[3] = CPolygon(6, 7, 1, 0);
        polys[4] = CPolygon(1, 7, 5, 3);
        polys[5] = CPolygon(6, 0, 2, 4);

        for (Int32 i=0; i < 6; i++) {
            CPolygon& p = polys[i];
            p.a += offPoints;
            p.b += offPoints;
            p.c += offPoints;
            p.d += offPoints;
        }

        // Adapt the texture for the current proxy object.
        if (tex) {
            // Create a selection tag for the polygons.
            SelectionTag* selTag = SelectionTag::Alloc(Tpolygonselection);
            BaseSelect* sel = selTag ? selTag->GetBaseSelect() : nullptr;
            if (sel != nullptr) {
                selTag->SetName("PROXY-" + String::IntToString(offPoints / 8));
                op->InsertTag(selTag);

                // Select the polygons we created.
                for (Int32 index=offPolys; index < offPolys + 6; index++) {
                    sel->Select(index);
                }
            }
            else
                SelectionTag::Free(selTag);

            // Create a clone of the texture tag and assign it to the
            // tag name.
            if (selTag) {
                BaseTag* clone = (BaseTag*) tex->GetClone(COPYFLAGS_0, nullptr);
                if (clone) {
                    GeData value(selTag->GetName());
                    clone->SetParameter(TEXTURETAG_RESTRICTION, value, DESCFLAGS_SET_0);
                    op->InsertTag(clone);
                }
            }
        }
    }

    static void update_object(
                BaseObject* op, maxon::BaseArray<ProxyTuple>& list,
                helpers::DependenceList& dep, Bool detailed, Bool recursive,
                BaseTag* tex=nullptr) {

        BaseTag* newTex = find_texture(op, tex != nullptr);
        if (newTex) tex = newTex;

        if (op->GetInfo() & OBJECT_INPUT) {
            list.Append(ProxyTuple(op, tex));
            return;
        }
        Bool use = (op->GetType() != Onull);

        if (detailed) {
            BaseObject* cache = op->GetDeformCache();
            if (!cache) cache = op->GetCache();
            if (cache) {
                update_object(cache, list, dep, false, true, tex);
                use = false;
            }
        }
        if (recursive) {
            for (BaseObject* child=op->GetDown(); child; child=child->GetNext()) {
                update_object(child, list, dep, detailed, true, tex);
            }
        }

        if (use) {
            Vector size = op->GetRad();
            if (size.GetLength() > 0.01) {
                dep.Add(op);
                list.Append(ProxyTuple(op, tex));
            }
        }
    }

    class ProxyGenData : public ObjectData {

        typedef ObjectData super;

        helpers::DependenceList m_dependenceList;

    public:

        static NodeData* alloc() { return NewObjClear(ProxyGenData); }

        virtual ~ProxyGenData() {
        }

    //
    // ObjectData overrides
    //

        virtual BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh) {
            if (!op || !hh)
                return nullptr;

            BaseContainer* bc = op->GetDataInstance();
            BaseDocument* doc = hh->GetDocument();
            const InExcludeData* includeList = nullptr;
            maxon::BaseArray<ProxyTuple> proxies;

            if (!bc || !doc)
                return nullptr;

            // Get the object from the InExclude list of the host object.
            includeList = static_cast<const InExcludeData*>(
                    bc->GetCustomDataType(PR1M_PROXYGEN_OBJECTS, CUSTOMDATATYPE_INEXCLUDE_LIST)
            );
            if (!includeList || includeList->GetObjectCount() <= 0)
                return BaseObject::Alloc(Onull);

            helpers::DependenceList list;
            list.Add(op);

            // Put all the objects into a new list.
            maxon::BaseArray<BaseObject*> objects;
            for (Int32 i=0; i < includeList->GetObjectCount(); i++) {

                // Get the current object from the InExcludeData.
                BaseObject* obj = static_cast<BaseObject*>(
                        includeList->ObjectFromIndex(doc, i)
                );
                if (!obj || !obj->IsInstanceOf(Obase))
                    continue;

                objects.Append(obj);
            }

            // Iterate over all items in the InExcludeData and fill
            // the proxies list.
            Bool recursive = bc->GetBool(PR1M_PROXYGEN_HIERARCHY);
            Bool detailed = bc->GetBool(PR1M_PROXYGEN_DETAILED);
            for (Int32 i=0; i < objects.GetCount(); i++) {
                BaseObject* op = objects[i];

                // If the detailed mode is enabled, we have to continue
                // recursively.
                update_object(op, proxies, list, detailed, recursive);
            }

            // Check if we could actually use the cache of the object.
            if (!list.Compare(m_dependenceList)) {
                BaseObject* cache = op->GetCache(hh);
                if (cache) return cache;
            }

            // Get the number of items that have been collected
            // and create a new PolygonObject from it.
            Int32 count = proxies.GetCount();
            if (count <= 0)
                return BaseObject::Alloc(Onull);

            PolygonObject* poly = PolygonObject::Alloc(count * 8, count * 6);
            if (!poly)
                return nullptr;

            // Get the point and polygon array from it that we will
            // write the proxy cubes to.
            Vector* points = poly->GetPointW();
            CPolygon* polys = poly->GetPolygonW();
            if (!points || !polys) {
                PolygonObject::Free(poly);
                return nullptr;
            }

            // Create a proxy cube for each gathered object.
            const Matrix mgi = c4d_apibridge::Invert(op->GetMg());
            for (Int32 index=0; index < count; index++) {

                Int32 offPoints = 8 * index;
                Int32 offPolys = 6 * index;
                make_proxy(poly, mgi, points, polys, offPoints, offPolys, proxies[index]);
            }

            poly->Message(MSG_UPDATE);
            return poly;
        }

      //
      // NodeData overrides
      //

        virtual Bool Init(GeListNode* node) {
            if (!super::Init(node) || !node) return false;

            BaseContainer* bc = ((BaseObject*) node)->GetDataInstance();
            if (!bc) return false;

            bc->SetBool(PR1M_PROXYGEN_HIERARCHY, true);
            bc->SetBool(PR1M_PROXYGEN_DETAILED, true);

            GeData value;
            value = GeData(CUSTOMDATATYPE_INEXCLUDE_LIST, DEFAULTVALUE);
            bc->SetData(PR1M_PROXYGEN_OBJECTS, value);

            return true;
        }

        virtual Bool GetDEnabling(GeListNode* node, const DescID& did, const GeData& t_data,
                    DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
            /*Int32 id = did[0].id;
            BaseContainer* bc = ((BaseObject*) node)->GetDataInstance();
            switch (id) {
                default:
                    break;
            }*/

            return super::GetDEnabling(node, did, t_data, flags, itemdesc);
        }

      private:

    };

    Bool register_proxy_generator() {
        menu::root().AddPlugin(IDS_MENU_OBJECTS, Opr1m_proxygen);
        return RegisterObjectPlugin(
            Opr1m_proxygen,
            GeLoadString(IDS_Opr1m_proxygen),
            PLUGINFLAG_HIDEPLUGINMENU | OBJECT_GENERATOR,
            ProxyGenData::alloc,
            "Opr1m_proxygen"_s,
            PR1MITIVE_ICON("Opr1m_proxygen"),
            PROXYGEN_VERSION);
    }

} // end namespace objects
} // end namespace pr1mitives

