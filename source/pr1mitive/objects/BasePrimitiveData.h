// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/defines.h>

#ifndef PR1MITIVE_OBJECTS_BASEPRIMITIVEDATA_H
#define PR1MITIVE_OBJECTS_BASEPRIMITIVEDATA_H

namespace pr1mitive {
namespace objects {

    // Implements basic functionality that is required for all object-plugins in the Pr1mitive
    // module (such as extended handle-drawing).
    class BasePrimitiveData : public ObjectData {

        typedef ObjectData super;

      protected:

        // This attribute saves the dirty-count of the host-object to compare it within
        // BasePrimitiveData::optmize_cache().
        LONG dirty_count;

        // These two static attributes will keep the default color-values for handles, selected and
        // not selected.
        static Vector color_handle;
        static Vector color_handle_selected;

      public:

      //
      // BasePrimitiveData ------------------------------------------------------------------------
      //

        // Static methods -------------------------------------------------------------------------

        // This method initializes static attributes on this class such as the *color_handle* and
        // *color_handle_selected* attribute. Should be called once.
        static void static_init();

        // This method performs cleanups for static-attributes of this class. Provided for
        // completeness, yet, as nothing needs to be freed.
        static void static_deinit();

        // New instanc methods --------------------------------------------------------------------

        BasePrimitiveData();

        // Returns the cache of the passed object if it hasn't changed since the last time the
        // method was called.
        BaseObject* optimize_cache(BaseObject* op);

        // Overwriteable Methods ------------------------------------------------------------------

        // Returns the number of how many handles are avaialble to the plugin-object.
        virtual LONG get_handle_count(BaseObject* op);

        // Fill in information into the passed HandleInfo object. Returns true when the handle was
        // obtained successfully, false if not.
        virtual Bool get_handle(BaseObject* op, LONG handle, HandleInfo* info);

        // Set parameters of the host-object based on the new position of a handle.
        virtual void set_handle(BaseObject* op, LONG handle, HandleInfo* info);

        // Draws a handle-dot into the viewport at the specified position of the handle. Can be
        // overwritten to customize the drawing. The vector in *info.position* is already set to
        // the point's position in global space.
        // The color is set in the BasePrimitiveData::Draw() procedure so it does not need to be
        // set here if no other color than the default is used.
        virtual void draw_handle(BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh, LONG handle, LONG hitid, HandleInfo* info, Vector& mp, Vector& size, Matrix& mg, Vector& color);

        // Draws additional stuff like a line from the origin to the handle. Can be overwritten
        // to customize stuff. The vector in *info.position* is already set to the point's position
        // in global space.
        // The color is set in the BasePrimitiveData::Draw() procedure so it does not need to be
        // set here if no other color than the default is used.
        virtual void draw_handle_customs(BaseObject* op, BaseDraw* bd, BaseDrawHelp* bh, LONG handle, LONG hitid, HandleInfo* info, Vector& mp, Vector& size, Matrix& mg, Vector& color);

      //
      // ObjectData -------------------------------------------------------------------------------
      //

        virtual DRAWRESULT Draw(BaseObject* op, DRAWPASS pass, BaseDraw* bd, BaseDrawHelp* bh);

        virtual LONG DetectHandle(BaseObject* op, BaseDraw* bd, LONG x, LONG y, QUALIFIER qualifier);

        virtual Bool MoveHandle(BaseObject* op, BaseObject* undo, const Vector& mouse_pos, LONG hitid, QUALIFIER qualifier, BaseDraw* bd);

    };

} // end namespace objects
} // end namespace pr1mitive

#endif // PR1MITIVE_OBJECTS_BASEPRIMITIVEDATA_H

