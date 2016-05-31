// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/debug.h>
#include <pr1mitive/helpers.h>
#include <pr1mitive/shapes/BaseComplexShape.h>
#include <nr/c4d/thread.h>
#include <mutex>

using nr::c4d::thread;

#ifdef PRMT
    #define BASECOMPLEXSHAPE_MULTITHREADING
#endif
#define BASECOMPLEXSHAPE_MAXPOINTSPERTHREAD 80000
#define BASECOMPLEXSHAPE_THREADINGCHUNKSIZE 80000
#define BASECOMPLEXSHAPE_LASTRUNINFO

namespace pr1mitive {
namespace shapes {

    struct ThreadingInfo {
        // Read Only
        BaseThread* bt;
        BaseObject* op;
        BaseComplexShape* shape;
        ComplexShapeInfo* info;
        Vector* points;
        LONG count;

        // Writable (lock by a mutex)
        std::mutex mutex;
        LONG pindex;
    };

    void generate_points(ThreadingInfo& data, LONG thread_index) {
        LONG const vdiv = data.info->vseg + 1;
        Real const umin = data.info->umin;
        Real const vmin = data.info->vmin;
        Real const udelta = data.info->udelta;
        Real const vdelta = data.info->vdelta;
        LONG const count = data.count;
        Vector* const points = data.points;

        while (true) { //!data.bt || !data.bt->TestBreak()) {
            data.mutex.lock();
            LONG const start = data.pindex;
            if (start >= count) {
                data.mutex.unlock();
                break;
            }
            LONG const end = maxon::Min(count, start + BASECOMPLEXSHAPE_THREADINGCHUNKSIZE);
            data.pindex = end;
            data.mutex.unlock();

            LONG x, i, j;
            Real u, v;
            for (x=start; x <= end; ++x) {
                i = x / vdiv;
                j = x % vdiv;
                u = umin + i * udelta;
                v = vmin + j * vdelta;
                points[x] = data.shape->calc_point(data.op, data.info, u, v, thread_index);
            }
        }
    }

    Bool BaseComplexShape::init_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
        info->useg = bc->GetLong(PR1M_COMPLEXSHAPE_USEGMENTS);
        info->vseg = bc->GetLong(PR1M_COMPLEXSHAPE_VSEGMENTS);
        info->optimize = bc->GetBool(PR1M_COMPLEXSHAPE_OPTIMIZE);
        info->optimize_passes = 1;
        info->multithreading = bc->GetBool(PR1M_COMPLEXSHAPE_MULTITHREADING);
        return true;
    }

    Bool BaseComplexShape::init_thread_activity(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info, LONG thread_count) {
        return true;
    }

    void BaseComplexShape::free_thread_activity(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info, LONG thread_count) {
    }

    void BaseComplexShape::post_process_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
    }

    void BaseComplexShape::free_calculation(BaseObject* op, BaseContainer* bc, ComplexShapeInfo* info) {
    }

    Vector BaseComplexShape::calc_point(BaseObject* op, ComplexShapeInfo* info, Real u, Real v, LONG thread_index) {
        return Vector(0);
    }

    Bool BaseComplexShape::Init(GeListNode* node) {
        if (not node) return false;
        BaseObject* op = (BaseObject*) node;
        BaseContainer* bc = op->GetDataInstance();

        // Initialize the object's parameters assuming it is implementing the
        // default description for complex shapes.
        bc->SetReal(PR1M_COMPLEXSHAPE_USEGMENTS, 20);
        bc->SetReal(PR1M_COMPLEXSHAPE_VSEGMENTS, 20);
        bc->SetBool(PR1M_COMPLEXSHAPE_OPTIMIZE, false);
        bc->SetBool(PR1M_COMPLEXSHAPE_MULTITHREADING, true);

        return true;
    }

    BaseObject* BaseComplexShape::GetVirtualObjects(BaseObject* op, HierarchyHelp* hh) {
        if (not op) return null;
        BaseContainer* bc = op->GetDataInstance();

        // Check if actually anything has changed and return the cache if
        // it is equal to the object we would compute afterwards.
        BaseObject* cache = optimize_cache(op);
        if (cache) {
            return cache;
        }

        // Initialize calculation.
        ComplexShapeInfo info;
        if (not init_calculation(op, bc, &info)) return null;

        // Perform some important checks for the info-structure.
        if (info.useg <= 0 or info.vseg <= 0) {
            PR1MITIVE_DEBUG_ERROR("ComplexShapeInfo was not initialized to a valid state in init_calculation(). End");
            return null;
        }

        // Compute the mesh-requirements.
        LONG n_points = (info.useg + 1) * (info.vseg + 1);
        LONG n_polys = info.useg * info.vseg;
        info.target = PolygonObject::Alloc(n_points, n_polys);

        // Emergency-break if the PolygonObject could not be allocated.
        if (not info.target) {
            PR1MITIVE_DEBUG_ERROR("PolygonObject could not be allocated. End");
            return null;
        }
        if (info.target->GetPointCount() != n_points) {
            PR1MITIVE_DEBUG_ERROR("PolygonObject's point-count does not match the required amount.");
            return null;
        }

        // Compute the space between each segment.
        info.udelta = (info.umax - info.umin) / info.useg;
        info.vdelta = (info.vmax - info.vmin) / info.vseg;

        // Obtain the write point- and polygon-pointer.
        Vector* points = info.target->GetPointW();
        CPolygon* polygons = info.target->GetPolygonW();

        if (not points or not polygons) {
            PR1MITIVE_DEBUG_ERROR("Points or Polygons of the target-mesh could not be retrieved. End");
            return null;
        }

        // Compute how many threads should actually be used.
        LONG thread_count = helpers::num_threads(n_points, BASECOMPLEXSHAPE_MAXPOINTSPERTHREAD, 1);
        if (thread_count <= 0) {
            PR1MITIVE_DEBUG_WARNING("Thread-count is invalid. Setting to 1");
            thread_count = 1;
        }
        if (not info.multithreading) {
            thread_count = 1;
        }
        #ifndef BASECOMPLEXSHAPE_MULTITHREADING
            thread_count = 1;
        #endif

        if (not init_thread_activity(op, bc, &info, thread_count)) {
            free_calculation(op, bc, &info);
            PolygonObject::Free(info.target);
            return null;
        }

        #ifdef BASECOMPLEXSHAPE_LASTRUNINFO
            LONG tstart = GeGetMilliSeconds();
        #endif

        ThreadingInfo thread_data = {hh->GetThread(), op, this, &info, points, n_points, {}, 0};
        maxon::BaseArray<thread> threads;
        if (thread_count == 1) {
            generate_points(thread_data, 0);
        }
        else {
            for (LONG i = 0; i < thread_count; ++i)
                threads.Append(thread(generate_points, std::ref(thread_data), i));
        }

        // Iterate over each polygon and construct the mesh'es polygons and the
        // UVW data.
        LONG poly_i = 0;
        LONG p1, p2, p3, p4;
        CPolygon poly;
        for (LONG i=0; i < info.useg; i++) {
            for (LONG j=0; j < info.vseg; j++) {
                // Compute the polygon's point-indecies.
                p1 = i * (info.vseg + 1) + j;
                p2 = p1 + 1;
                p3 = (i + 1) * (info.vseg + 1) + j + 1;
                p4 = p3 - 1;

                // Construct a polygon, with inverted point-order if normals should
                // be inverted.
                if (info.rotate_polys) {
                    if (info.inverse_normals) poly = CPolygon(p1, p4, p3, p2);
                    else poly = CPolygon(p2, p3, p4, p1);
                }
                else {
                    if (info.inverse_normals) poly = CPolygon(p4, p3, p2, p1);
                    else poly = CPolygon(p1, p2, p3, p4);
                }

                // Set the polygon to the object and increase the polygon-index.
                polygons[poly_i] = poly;
                poly_i++;
            }
        }

        // Watch out for a Phong-tag if desired and insert a copy of it onto the
        // generated mesh.
        if (info.watch_for_phong) {
            BaseTag* tag = op->GetFirstTag();
            while (tag) {
                if (tag->IsInstanceOf(Tphong)) {
                    tag = (BaseTag*) tag->GetClone(COPYFLAGS_0, null);
                    info.target->InsertTag(tag);
                    break;
                }
                tag = tag->GetNext();
            }
        }

        // Generate a UVW Tag if desired.
        if (info.generate_uvw) {
            info.uvw_dest = helpers::make_planar_uvw(info.useg, info.vseg, info.inverse_normals, info.flip_uvw_x, info.flip_uvw_y);
            if (info.uvw_dest) info.target->InsertTag(info.uvw_dest);
        }

        // When threading was enabled, we will wait until the threads
        // have finished and deallocate them.
        for (auto& thread : threads)
            thread.join();
        free_thread_activity(op, bc, &info, thread_count);

        // Optimize passes.
        if (info.optimize) {
            for (LONG i=0; i < info.optimize_passes; i++) {
                Bool success = helpers::optimize_object(info.target, info.optimize_treshold);
                if (not success) PR1MITIVE_DEBUG_ERROR("MCOMMAND_OPTIMIZE pass #" + LongToString(i) + " failed.");
            }
        }

        #ifdef BASECOMPLEXSHAPE_LASTRUNINFO
            String str;
            LONG delta = GeGetMilliSeconds() - tstart;
            if (info.multithreading) {
                str = GeLoadString(IDS_LASTRUN_MULTI, LongToString(delta), LongToString(thread_count));
            }
            else {
                str = GeLoadString(IDS_LASTRUN, LongToString(delta));
            }
            bc->SetString(PR1M_COMPLEXSHAPE_LASTRUN, str);
            dirty_count = op->GetDirty(DIRTYFLAGS_DATA);
        #endif

        // Invoke the post-processor method.
        post_process_calculation(op, bc, &info);

        // Optional cleanups, etc.
        free_calculation(op, bc, &info);

        // Maybe post_process_object() made info.target invalid.
        if (not info.target) return null;

        // Inform the mesh about the changes.
        info.target->Message(MSG_UPDATE);
        return info.target;
    }

    Bool BaseComplexShape::Message(GeListNode* node, LONG type, void* ptr) {
        if (not node) return false;
        BaseObject* op = (BaseObject*) node;

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

            // Make sure no invalid values are set in the description.
            case MSG_DESCRIPTION_POSTSETPARAMETER: {
                BaseContainer* bc = op->GetDataInstance();

                // Obtain the description-ID from the message.
                DescriptionPostSetValue* data = (DescriptionPostSetValue*) ptr;
                const DescID& id = *data->descid;
                LONG rid = id[0].id;

                // Adjust parameters.
                if (rid is PR1M_COMPLEXSHAPE_USEGMENTS or rid is PR1M_COMPLEXSHAPE_VSEGMENTS) {
                    LONG value = bc->GetLong(rid);
                    bc->SetLong(rid, helpers::limit_min<LONG>(value, 1));
                }
                break;

            }
        }

        return super::Message(node, type, ptr);
    }


    Bool register_complexshape_base() {
        return RegisterDescription(Opr1m_complexshape, "Opr1m_complexshape");
    }

} // end namespace shapes
} // end namespace pr1mitive

