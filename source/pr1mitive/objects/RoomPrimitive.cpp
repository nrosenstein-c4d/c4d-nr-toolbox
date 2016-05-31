// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/defines.h>
#include <pr1mitive/debug.h>
#include <pr1mitive/helpers.h>
#include <pr1mitive/activation.h>
#include <pr1mitive/objects/BasePrimitiveData.h>
#include "res/description/Opr1m_room.h"
#include "menu.h"

using namespace pr1mitive::helpers;

namespace pr1mitive {
namespace objects {

    class RoomPrimitive : public BasePrimitiveData {

        typedef BasePrimitiveData super;

      private:

        // These will contain an array of names that will be assigned to the internal
        // polygon-selections of the object.
        static const char* selection_names_simple[];
        static const char* selection_names_extended[];

        // Handle IDs
        static const char HANDLE_WIDTH = 0;
        static const char HANDLE_HEIGHT = 1;
        static const char HANDLE_DEPTH = 2;
        static const char HANDLE_THICKXP = 3;
        static const char HANDLE_THICKYP = 4;
        static const char HANDLE_THICKZP = 5;
        static const char HANDLE_THICKXN = 6;
        static const char HANDLE_THICKYN = 7;
        static const char HANDLE_THICKZN = 8;

        // New color for the lines of the thickness-handles.
        static Vector color_handle_thicknessline;

      public:

        static NodeData* alloc() { return gNew(RoomPrimitive); }

        static void static_init() {
            color_handle_thicknessline = GetViewColor(VIEWCOLOR_VERTEXEND);
        }

        // Set the containers information based on the passed ID and the data passed, limited to
        // the object's information (fillet-radius, thickness, etc.).
        static void set_information(BaseContainer* bc, LONG id, Vector& size, Vector& thickp, Vector& thickn);

      //
      // BasePrimitiveData ------------------------------------------------------------------------
      //

        LONG get_handle_count(BaseObject* op);

        Bool get_handle(BaseObject* op, LONG handle, HandleInfo* info);

        void set_handle(BaseObject* op, LONG handle, HandleInfo* info);

        void draw_handle_customs(BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh, LONG handle, LONG hitid, HandleInfo* info, Vector& mp, Vector& size, Matrix& mg, Vector& color);

      //
      // ObjectData -------------------------------------------------------------------------------
      //

        Bool Init(GeListNode* node);

        Bool GetDDescription(GeListNode* node, Description* desc, DESCFLAGS_DESC& flags);

        Bool GetDEnabling(GeListNode* node, const DescID& id, const GeData& data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc);

        BaseObject* GetVirtualObjects(BaseObject* op, HierarchyHelp* hh);

        Bool Message(GeListNode* node, LONG type, void* ptr);

        void GetDimension(BaseObject* op, Vector* mp, Vector* size);

    };

  //
  // RoomPrimitive --------------------------------------------------------------------------------
  //

    const char* RoomPrimitive::selection_names_simple[]   = { "Walls", "", "", "", "Ceiling", "Floor", "R1", "", "R3" };
    const char* RoomPrimitive::selection_names_extended[] = { "Z-", "X+", "Z+", "X-", "Y+", "Y-", "R1", "R2", "R3" };
    Vector RoomPrimitive::color_handle_thicknessline;

    void RoomPrimitive::set_information(BaseContainer* bc, LONG id, Vector& size, Vector& thickp, Vector& thickn) {
        Real fillet;
        if (bc->GetBool(PR1M_ROOM_DOFILLET)) fillet = bc->GetReal(PR1M_ROOM_FILLETRAD) * 2;
        else fillet = 0;

        // We now handle the three accepted cases seperately.
        switch (id) {
            case PR1M_ROOM_SIZE:
                size.x = limit_min<Real>(size.x, thickp.x + thickn.x);
                size.y = limit_min<Real>(size.y, thickp.y + thickn.y);
                size.z = limit_min<Real>(size.z, thickp.z + thickn.z);
                bc->SetVector(PR1M_ROOM_SIZE, size);
                break;
            case PR1M_ROOM_THICKNESSP:
                thickp.x = limit<Real>(0, thickp.x, size.x - thickn.x - fillet);
                thickp.y = limit<Real>(0, thickp.y, size.y - thickn.y - fillet);
                thickp.z = limit<Real>(0, thickp.z, size.z - thickn.z - fillet);
                bc->SetVector(PR1M_ROOM_THICKNESSP, thickp);
                break;
            case PR1M_ROOM_THICKNESSN:
                thickn.x = limit<Real>(0, thickn.x, size.x - thickp.x - fillet);
                thickn.y = limit<Real>(0, thickn.y, size.y - thickp.y - fillet);
                thickn.z = limit<Real>(0, thickn.z, size.z - thickp.z - fillet);
                bc->SetVector(PR1M_ROOM_THICKNESSN, thickn);
                break;
            case PR1M_ROOM_FILLETRAD:
                fillet = limit<Real>(0, fillet, size.x - thickp.x - thickn.x);
                fillet = limit<Real>(0, fillet, size.y - thickp.y - thickn.y);
                fillet = limit<Real>(0, fillet, size.z - thickp.z - thickn.z);
                bc->SetReal(PR1M_ROOM_FILLETRAD, fillet / 2);
                break;
            default:
                PR1MITIVE_DEBUG_ERROR("Invalid ID passed: " + LongToString(id));
                break;
        }
    }

  //
  // RoomPrimitive :: BasePrimitiveData -----------------------------------------------------------
  //

    LONG RoomPrimitive::get_handle_count(BaseObject* op) {
        BaseContainer* bc = op->GetDataInstance();
        // Three handles for the room's size when the thickness handles should
        // not be shown. Otherwise, 6 additional handles for the thickness.
        if (bc->GetBool(PR1M_ROOM_DISPLAYTHICKNESSHANDLES)) return 9;
        else return 3;
    }

    Bool RoomPrimitive::get_handle(BaseObject* op, LONG handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        // Obtain the information needed for the handles.
        Vector size = bc->GetVector(PR1M_ROOM_SIZE) * 0.5;
        Vector thickp = bc->GetVector(PR1M_ROOM_THICKNESSP);
        Vector thickn = bc->GetVector(PR1M_ROOM_THICKNESSN);

        // References to the information that needs to be filled so we don't have
        // to write that over and over again.
        Vector& point = info->position;
        Vector& diret = info->direction;
        switch(handle) {
            // Size handles
            case HANDLE_WIDTH:
                point.x = size.x;
                diret.x = 1;
                break;
            case HANDLE_HEIGHT:
                point.y = size.y;
                diret.y = 1;
                break;
            case HANDLE_DEPTH:
                point.z = size.z;
                diret.z = 1;
                break;

            // X handles
            case HANDLE_THICKXP:
                point.x = size.x - thickp.x;
                diret.x = 1;
                break;
            case HANDLE_THICKXN:
                point.x = -size.x + thickn.x;
                diret.x = 1;
                break;

            // Y handles
            case HANDLE_THICKYP:
                point.y = size.y - thickp.y;
                diret.y = 1;
                break;
            case HANDLE_THICKYN:
                point.y = -size.y + thickn.y;
                diret.y = 1;
                break;

            // Z handles
            case HANDLE_THICKZP:
                point.z = size.z - thickp.z;
                diret.z = 1;
                break;
            case HANDLE_THICKZN:
                point.z = -size.z + thickn.z;
                diret.z = 1;
                break;

            default:
                return false;
        }

        return true;
    }

    void RoomPrimitive::set_handle(BaseObject* op, LONG handle, HandleInfo* info) {
        BaseContainer* bc = op->GetDataInstance();
        // A reference to the handle's position, to save some writing..
        Vector& point = info->position;

        // This information will be modified in the switch-case clause and set
        // back to the object's container.
        Vector size = bc->GetVector(PR1M_ROOM_SIZE);
        Vector thickp = bc->GetVector(PR1M_ROOM_THICKNESSP);
        Vector thickn = bc->GetVector(PR1M_ROOM_THICKNESSN);

        switch (handle) {
            // Size handles
            case HANDLE_WIDTH:
                size.x = point.x * 2;
                set_information(bc, PR1M_ROOM_SIZE, size, thickp, thickn);
                break;
            case HANDLE_HEIGHT:
                size.y = point.y * 2;
                set_information(bc, PR1M_ROOM_SIZE, size, thickp, thickn);
                break;
            case HANDLE_DEPTH:
                size.z = point.z * 2;
                set_information(bc, PR1M_ROOM_SIZE, size, thickp, thickn);
                break;

            // X handles
            case HANDLE_THICKXP:
                thickp.x = (size.x / 2) - point.x;
                set_information(bc, PR1M_ROOM_THICKNESSP, size, thickp, thickn);
                break;
            case HANDLE_THICKXN:
                thickn.x = point.x + (size.x / 2);
                set_information(bc, PR1M_ROOM_THICKNESSN, size, thickp, thickn);
                break;

            // Y handles
            case HANDLE_THICKYP:
                thickp.y = (size.y / 2) - point.y;
                set_information(bc, PR1M_ROOM_THICKNESSP, size, thickp, thickn);
                break;
            case HANDLE_THICKYN:
                thickn.y = point.y + (size.y / 2);
                set_information(bc, PR1M_ROOM_THICKNESSN, size, thickp, thickn);
                break;

            // Z handles
            case HANDLE_THICKZP:
                thickp.z = (size.z / 2) - point.z;
                set_information(bc, PR1M_ROOM_THICKNESSP, size, thickp, thickn);
                break;
            case HANDLE_THICKZN:
                thickn.z = point.z + (size.z / 2);
                set_information(bc, PR1M_ROOM_THICKNESSN, size, thickp, thickn);
                break;
        }
    }

    void RoomPrimitive::draw_handle_customs(BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh, LONG handle, LONG hitid, HandleInfo* info, Vector& mp, Vector& size, Matrix& mg, Vector& color) {

        // Default customs for the Size-handles.
        switch (handle) {
            // Size handles
            case HANDLE_WIDTH:
            case HANDLE_HEIGHT:
            case HANDLE_DEPTH:
                super::draw_handle_customs(op, bd, bh, handle, hitid, info, mp, size, mg, color);
                return;
            default:
                break;
        }

        // Custom handle-drawing for the thickness-handles.
        // ------------------------------------------------

        // BaseContainer* bc = op->GetDataInstance();
        // Vector thickp = bc->GetVector(PR1M_ROOM_THICKNESSP);
        // Vector thickn = bc->GetVector(PR1M_ROOM_THICKNESSN);
        Vector origin;
        switch (handle) {
            // X Handles
            case HANDLE_THICKXP:
                origin.x = size.x;
                break;
            case HANDLE_THICKXN:
                origin.x = -size.x;
                break;

            // Y Handles
            case HANDLE_THICKYP:
                origin.y = size.y;
                break;
            case HANDLE_THICKYN:
                origin.y = -size.y;
                break;

            // Z Handles
            case HANDLE_THICKZP:
                origin.z = size.z;
                break;
            case HANDLE_THICKZN:
                origin.z = -size.z;
                break;

            default:
                return;
        }

        bd->SetPen(color_handle_thicknessline);
        bd->DrawLine(origin + mp, info->position, 0);
    }

  //
  // RoomPrimitive :: BasePrimitiveData :: ObjectData ---------------------------------------------
  //

    Bool RoomPrimitive::Init(GeListNode* node) {
        if (not node) return false;
        BaseObject* op = (BaseObject*) node;
        BaseContainer* bc = op->GetDataInstance();

        bc->SetVector(PR1M_ROOM_SIZE, Vector(1000, 400, 1000));
        bc->SetVector(PR1M_ROOM_THICKNESSP, Vector(20));
        bc->SetVector(PR1M_ROOM_THICKNESSN, Vector(20));
        bc->SetBool(PR1M_ROOM_DOFILLET, false);
        bc->SetReal(PR1M_ROOM_FILLETRAD, 3.0);
        bc->SetLong(PR1M_ROOM_FILLETSUB, 5);
        bc->SetBool(PR1M_ROOM_DISPLAYTHICKNESSHANDLES, false);
        bc->SetLong(PR1M_ROOM_TEXTUREMODE, PR1M_ROOM_TEXTUREMODE_SIMPLE);
        return true;
    }

    Bool RoomPrimitive::GetDDescription(GeListNode* node, Description* desc, DESCFLAGS_DESC& flags) {
        if (not desc) return false;
        if (not desc->LoadDescription(Opr1m_room)) return false;

        BaseContainer* bc;
        bc = desc->GetParameterI(PR1M_ROOM_SIZE, nullptr);
        if (bc) bc->SetString(DESC_SHORT_NAME, "S");
        bc = desc->GetParameterI(PR1M_ROOM_THICKNESSP, nullptr);
        if (bc) bc->SetString(DESC_SHORT_NAME, "P");
        bc = desc->GetParameterI(PR1M_ROOM_THICKNESSN, nullptr);
        if (bc) bc->SetString(DESC_SHORT_NAME, "M");
        flags |= DESCFLAGS_DESC_LOADED;

        return true;
    }

    Bool RoomPrimitive::GetDEnabling(GeListNode* node, const DescID& id, const GeData& data, DESCFLAGS_ENABLE flags, const BaseContainer* itemdesc) {
        if (not node) return false;
        BaseObject* op = (BaseObject*) node;
        BaseContainer* bc = op->GetDataInstance();
        if (not bc) return false;

        LONG rid = id[0].id;
        switch (rid) {
            case PR1M_ROOM_FILLETRAD:
            case PR1M_ROOM_FILLETSUB:
                return bc->GetBool(PR1M_ROOM_DOFILLET);
            default:
                break;
        };

        return super::GetDEnabling(node, id, data, flags, itemdesc);
    }

    BaseObject* RoomPrimitive::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh) {
        if (not op) return null;

        // We will not continue if nothing has changed at all.
        BaseObject* cache = optimize_cache(op);
        if (cache) return cache;

        // First thing we need to do is to obtain all the information that we need
        // to create the internal two cubes for this object. Always obtaining it
        // from the container is too slow and too much code.
        BaseContainer* bc = op->GetDataInstance();
        Vector size = bc->GetVector(PR1M_ROOM_SIZE);
        Vector thickp = bc->GetVector(PR1M_ROOM_THICKNESSP);
        Vector thickn = bc->GetVector(PR1M_ROOM_THICKNESSN);
        Bool dofillet = bc->GetBool(PR1M_ROOM_DOFILLET);
        Real filletrad = bc->GetReal(PR1M_ROOM_FILLETRAD);
        LONG filletsub = bc->GetLong(PR1M_ROOM_FILLETSUB);
        LONG texturemode = bc->GetLong(PR1M_ROOM_TEXTUREMODE);

        // Now, we're going to create the two cubes.
        BaseObject* o_cube1 = BaseObject::Alloc(Ocube);
        if (not o_cube1) return null;
        BaseObject* o_cube2 = BaseObject::Alloc(Ocube);
        if (not o_cube2) {
            BaseObject::Free(o_cube1);
            return null;
        }

        // Set information that both cubes require.
        BaseContainer* bcc1, *bcc2;
        bcc1 = o_cube1->GetDataInstance();
        bcc2 = o_cube2->GetDataInstance();
        BaseContainer* containers[2] = { bcc1, bcc2 };
        for (int i=0; i < 2; i++) {
            BaseContainer* bc = containers[i];
            bc->SetVector(PRIM_CUBE_LEN, size);
            bc->SetLong(PRIM_CUBE_SUBX, 1);
            bc->SetLong(PRIM_CUBE_SUBY, 1);
            bc->SetLong(PRIM_CUBE_SUBZ, 1);
            bc->SetBool(PRIM_CUBE_DOFILLET, false);
        }

        // Set the information for the inner cube.
        bcc2->SetVector(PRIM_CUBE_LEN, (size - (thickn + thickp)));
        bcc2->SetBool(PRIM_CUBE_DOFILLET, dofillet);
        bcc2->SetReal(PRIM_CUBE_FRAD, filletrad);
        bcc2->SetLong(PRIM_CUBE_SUBF, filletsub);

        // Convert the inner cube to a polygon-object by calling the cube's
        // ObjectData::GetVirtualObjects() method.
        ObjectData*   cubedata   = o_cube2->GetNodeData<ObjectData>();
        OBJECTPLUGIN* cubeplugin = C4D_RETRIEVETABLE(OBJECTPLUGIN*, cubedata);
        BaseObject*   new_cube   = (cubedata->*(cubeplugin->GetVirtualObjects))(o_cube2, hh);
        if (not new_cube) {
            PR1MITIVE_DEBUG_ERROR("Cube ObjectData::GetVirtualObjects() returned null.");
            BaseObject::Free(o_cube1);
            BaseObject::Free(o_cube2);
            return null;
        }

        BaseObject::Free(o_cube2);
        o_cube2 = new_cube;

        // Offset the new cube so that it's distance from the outter cube is
        // equal to the defined thickness.
        o_cube2->SetRelPos((thickn - thickp) * 0.5);

        // Inverse the inner cubes normals.
        ModelingCommandData mdata;
        mdata.op = o_cube2;
        Bool success = SendModelingCommand(MCOMMAND_REVERSENORMALS, mdata);
        if (not success) {
            PR1MITIVE_DEBUG_ERROR("MCOMMAND_REVERSENORMALS failed.");
        }

        // Create the selection-sets for the inner cube.
        int selectioncount;
        if (not dofillet) {
            filletsub = 0;
            selectioncount = 6;
        }
        else {
            selectioncount = 9;
        }
        BaseSelect** selections = new BaseSelect*[selectioncount];
        for (int i=0; i < selectioncount; i++) {
            selections[i] = BaseSelect::Alloc();
            if (not selections[i]) {
                PR1MITIVE_DEBUG_ERROR("Could not allocate BaseSelect object.");
                goto end;
            }
        }

        success = make_cube_selections(filletsub, selections);
        if (not success) {
            PR1MITIVE_DEBUG_ERROR("make_cube_selections() failed.");
            goto end;
        }

        // The names of the selections will be stored here, associative to the
        // selections in the *selections* array.
        const char** names;

        // In simple-mode, we need to join some of the selections into one.
        if (texturemode isnot PR1M_ROOM_TEXTUREMODE_EXTENDED) {
            for (int i=1; i < 4; i++) {
                selections[0]->Merge(selections[i]);
                BaseSelect::Free(selections[i]);
                selections[i] = null;
            }

            if (dofillet) {
                selections[6]->Merge(selections[7]);
                BaseSelect::Free(selections[7]);
                selections[7] = null;
            }

            names = selection_names_simple;
        }
        else
            names = selection_names_extended;

        // Insert the selections on the inner cube so they can be accesed by
        // texture-tags.
        for (int i=0; i < selectioncount; i++) {
            if (not selections[i]) continue;

            SelectionTag* tag = (SelectionTag*) o_cube2->MakeTag(Tpolygonselection);
            if (not tag) {
                PR1MITIVE_DEBUG_ERROR("Tpolygonselection could not be created.");
                goto end;
            }

            selections[i]->CopyTo(tag->GetBaseSelect());
            tag->SetName(names[i]);
        }

      end:
        // Free the selections previously allocated.
        for (int i=0; i < selectioncount; i++) {
            if (selections[i]) BaseSelect::Free(selections[i]);
        }
        delete selections;

        // Search for a Phong-Tag on our Room-Object and insert it onto the inner
        // cube.
        BaseTag* tag = op->GetFirstTag();
        while (tag) {
            if (tag->IsInstanceOf(Tphong)) {
                o_cube2->InsertTag((BaseTag*) tag->GetClone(COPYFLAGS_0, null));
                break;
            }
            tag = tag->GetNext();
        }

        // Create a Null-Object to group the cubes under one object, set their
        // names and return them.
        BaseObject* root = BaseObject::Alloc(Onull);
        if (not root) {
            BaseObject::Free(o_cube1);
            BaseObject::Free(o_cube2);
            return null;
        }
        o_cube1->SetName("Outter");
        o_cube2->SetName("Inner");
        o_cube1->InsertUnder(root);
        o_cube2->InsertUnder(root);

        return root;
    }

    Bool RoomPrimitive::Message(GeListNode* node, LONG type, void* ptr) {
        if (not node) return false;
        BaseObject* op = (BaseObject*) node;
        BaseContainer* bc = op->GetDataInstance();

        switch (type) {
            // Invoked on creation of the object.
            case MSG_MENUPREPARE: {
                // Create a Phong-Tag on our object. This phong tag will be retrieve
                // in GetVirtualObjects() and inserted into the virtual objects.
                BaseTag* tag = op->MakeTag(Tphong);
                if (not tag) return false;
                BaseContainer* tbc = tag->GetDataInstance();
                if (not tbc) return false;
                tbc->SetBool(PHONGTAG_PHONG_ANGLELIMIT, true);
                return true;
            }

            // Invoked when a description parameter is modified. We need to limit
            // several parameters in the description here.
            case MSG_DESCRIPTION_POSTSETPARAMETER: {
                DescriptionPostSetValue* data = (DescriptionPostSetValue*) ptr;
                const DescID& id = *data->descid;

                // We will limit the values for the size and thickness using
                // the set_information() function.
                if (id is PR1M_ROOM_SIZE or id is PR1M_ROOM_THICKNESSP or
                    id is PR1M_ROOM_THICKNESSN or id is PR1M_ROOM_FILLETRAD) {
                    Vector size = bc->GetVector(PR1M_ROOM_SIZE);
                    Vector thickp = bc->GetVector(PR1M_ROOM_THICKNESSP);
                    Vector thickn = bc->GetVector(PR1M_ROOM_THICKNESSN);
                    set_information(bc, id[0].id, size, thickp, thickn);
                }
                break;
            }
        }

        // Ask the parent-class for what to return.
        return super::Message(node, type, ptr);
    }

    void RoomPrimitive::GetDimension(BaseObject* op, Vector* mp, Vector* size) {
        if (not op) return;
        BaseContainer* bc = op->GetDataInstance();

        *mp = Vector(0);
        *size = bc->GetVector(PR1M_ROOM_SIZE) * 0.5;
    }


    Bool register_room() {
        menu::root().AddPlugin(IDS_MENU_OBJECTS, Opr1m_room);
        RoomPrimitive::static_init();
        return RegisterObjectPlugin(
                Opr1m_room,
                GeLoadString(IDS_Opr1m_room),
                PLUGINFLAG_HIDEPLUGINMENU | OBJECT_GENERATOR,
                RoomPrimitive::alloc,
                "Opr1m_room", PR1MITIVE_ICON("Opr1m_room"), 0);
    }

} // end namespace objects
} // end namespace pr1mitive

