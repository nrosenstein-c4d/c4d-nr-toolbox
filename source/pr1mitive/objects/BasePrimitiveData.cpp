// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/objects/BasePrimitiveData.h>

namespace pr1mitive {
namespace objects {

    Vector BasePrimitiveData::color_handle;
    Vector BasePrimitiveData::color_handle_selected;

    void BasePrimitiveData::static_init() {
        color_handle = GetViewColor(VIEWCOLOR_ACTIVEPOINT);
        color_handle_selected = GetViewColor(VIEWCOLOR_SELECTION_PREVIEW);
    }

    void BasePrimitiveData::static_deinit() {
    }

    BasePrimitiveData::BasePrimitiveData()
    : super() {
        dirty_count = -1;
    }

    BaseObject* BasePrimitiveData::optimize_cache(BaseObject* op) {
        LONG new_dirty_count = op->GetDirty(DIRTYFLAGS_DATA);
        if (new_dirty_count != dirty_count) {
            dirty_count = new_dirty_count;
            return null;
        }
        return op->GetCache();
    }

    LONG BasePrimitiveData::get_handle_count(BaseObject* op) {
        return 0;
    }

    Bool BasePrimitiveData::get_handle(BaseObject* op, LONG handle, HandleInfo* info) {
        return false;
    }

    void BasePrimitiveData::set_handle(BaseObject* op, LONG handle, HandleInfo* info) {
    }

    void BasePrimitiveData::draw_handle(BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh, LONG handle, LONG hitid, HandleInfo* info, Vector& mp, Vector& size, Matrix& mg, Vector& color) {
        bd->DrawHandle(info->position, DRAWHANDLE_BIG, NOCLIP_D | NOCLIP_Z);
    }

    void BasePrimitiveData::draw_handle_customs(BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh, LONG handle, LONG hitid, HandleInfo* info, Vector& mp, Vector& size, Matrix& mg, Vector& color){
        switch (info->type) {
            case HANDLECONSTRAINTTYPE_FREE:
                break;
            case HANDLECONSTRAINTTYPE_LINEAR:
                bd->DrawLine(info->position, mp, NOCLIP_D | NOCLIP_Z);
                break;
            case HANDLECONSTRAINTTYPE_PLANAR:
                break;
            case HANDLECONSTRAINTTYPE_RADIAL:
                break;
            case HANDLECONSTRAINTTYPE_SPHERICAL:
                break;
            case HANDLECONSTRAINTTYPE_INVALID:
            default:
                break;
        };
    }

    DRAWRESULT BasePrimitiveData::Draw(BaseObject* op, DRAWPASS pass, BaseDraw* bd, BaseDrawHelp* bh) {
        // We only require the DRAWPASS_HANDLES pass.
        if (not op) return DRAWRESULT_ERROR;
        if (pass isnot DRAWPASS_HANDLES) return DRAWRESULT_OK;

        bd->SetMatrix_Matrix(op, bh->GetMg());
        Matrix mg = op->GetMg();
        Vector mp, size;
        GetDimension(op, &mp, &size);

        // Iterate over the object's handles and draw them into the viewport.
        LONG hitid = op->GetHighlightHandle(bd);
        for (int i=0; i < get_handle_count(op); i++) {
            HandleInfo info;
            info.center = mp;
            if (not get_handle(op, i, &info)) continue;

            // Offset the handle about the BB-center. Matrix-multiplication is not necessary due
            // to the bd->SetMatrix_Matrix() call.
            mp = info.center;
            info.position += mp;

            Vector* color;
            if (i is hitid) color = &color_handle_selected;
            else color = &color_handle;

            bd->SetPen(*color);
            draw_handle(op, bd, bh, i, hitid, &info, mp, size, mg, *color);
            draw_handle_customs(op, bd, bh, i, hitid, &info, mp, size, mg, *color);
        }

        return DRAWRESULT_OK;
    }

    LONG BasePrimitiveData::DetectHandle(BaseObject* op, BaseDraw* bd, LONG x, LONG y, QUALIFIER qualifier) {
        // CTRL is not allowed.
        if (not op) return NOTOK;
        if (qualifier & QUALIFIER_CTRL) return NOTOK;

        Matrix matrix = op->GetMg();
        LONG handle   = NOTOK;
        Bool shift    = qualifier & QUALIFIER_SHIFT;

        Vector mp, size;
        GetDimension(op, &mp, &size);

        // Iterate over each handle and check if one of theese match at
        // the current mouse-position.
        for (LONG i=0; i < get_handle_count(op); i++) {
            HandleInfo info;
            info.center = mp;
            if (not get_handle(op, i, &info)) continue;

            // Offset the handle by the object's bounding-box center.
            mp = info.center;
            info.position += mp;

            if (bd->PointInRange(matrix * info.position, x, y)) {
                handle = i;

                // Use this handle when SHIFT is not pressed (which means
                // to use the first handle found). Otherwise, the last handle
                // will be used.
                if (shift) break;
            }
        }

        return handle;
    }

    Bool BasePrimitiveData::MoveHandle(BaseObject* op, BaseObject* undo, const Vector& mouse_pos, LONG hitid, QUALIFIER qualifier, BaseDraw* bd) {
        if (not op) return false;

        Vector mp, size;
        GetDimension(op, &mp, &size);

        // Get the identified handle. Do not continue if get_handle() returned
        // False.
        HandleInfo info;
        info.center = mp;
        if (not get_handle(op, hitid, &info)) return true;

        mp = info.center;
        info.position += mp;

        // Compute the handles new position.
        Matrix matrix = op->GetUpMg() * undo->GetMl();
        info.position = info.CalculateNewPosition(bd, matrix, mouse_pos);

        // Undo the handle-offset.
        info.position -= mp;

        // Invoke the procedure that implements the data-processing of the handle.
        set_handle(op, hitid, &info);
        return true;
    }

} // end namespace objects
} // end namespace pr1mitive

