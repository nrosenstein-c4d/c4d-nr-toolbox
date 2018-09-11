// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <thread>
#include <pr1mitive/defines.h>

#if API_VERSION < 14000
    #include <ge_dynamicarray.h>
#endif

#ifndef PR1MITIVE_HELPERS_H
#define PR1MITIVE_HELPERS_H

namespace pr1mitive {
namespace helpers {

    // Obtain the object's virtual method-table and automatically cast it into the given type.
    #ifndef C4D_RETRIEVETABLE
    #define C4D_RETRIEVETABLE(type, x) ((type)(C4DOS.Bl->RetrieveTableX(x, 0)))
    #endif

    // ArrayClass definition depending on the API Version.
    #if API_VERSION < 14000
        template <class T> class ArrayClass : public GeDynamicArray<T>
    #elif API_VERSION < 15000
        template <class T> class ArrayClass : public maxon::BaseArray<T>
    #else
        template <class T> class ArrayClass : public maxon::BaseArray<T>
    #endif
    {

    public:

        #if API_VERSION < 14000
            T* Append(const T& x) {
                if (Push(x)) {
                    return & operator [] (GetCount() - 1);
                }
                else return nullptr;
            }

            void Reset() {
                FreeArray();
            }

            Bool CopyFrom(const ArrayClass<T>& other) {
                new (this) ArrayClass(other);
            }
            FUUFUCKSHIT
        #endif

            T& Get(Int32 index) {
                return this->operator [] (index);
            }

            const T& Get(Int32 index) const {
                return this->operator [] (index);
            }

    };

    // Convert a vector to a string. Naming convention is different to
    // better fit in with the C4D API.
    inline String VectorToString(const Vector& v) {
        return String("Vector(") + String::FloatToString(v.x) + String(", ") + String::FloatToString(v.y) + String(", ") + String::FloatToString(v.z) + String(")");
    }

    // Convert a matrix to a string. Naming convention is different to better fit
    // in with the C4D API.
    inline String MatrixToString(const Matrix& m) {
        String nl("\n");
        String c(",");
        using namespace c4d_apibridge::M;
        return String("Matrix{ v1:  ") + VectorToString(Mv1(m)) + c + nl +
               String("        v2:  ") + VectorToString(Mv2(m)) + c + nl +
               String("        v3:  ") + VectorToString(Mv3(m)) + c + nl +
               String("        off: ") + VectorToString(Moff(m)) + String("}");
    }

    // Creates a set of 9 selections that select different parts of a
    // cube-object. The passed number is the number of roundings the cube-object
    // has. The passed BaseSelect pointer must point to an array of nine
    // elements if ``n != 0``. If ``n == 0``, *selections* can be six
    // BaseSelect elements.
    Bool make_cube_selections(int n, BaseSelect** selections);

    // Creates an UVW Tag that distributes in a planar grid.
    UVWTag* make_planar_uvw(int useg, int vseg, Bool inverse_normals=false, Bool flipx=false, Bool flipy=false);

    // Optimizes the polygon- or spline-object using a modeling-command.
    Bool optimize_object(BaseObject* op, Float treshold);

    // Return a vector whos' components are mixed to produce the lowest values.
    inline Vector vcompmin(const Vector& a, const Vector& b) {
        Vector d = a;
        if (d.x > b.x) d.x = b.x;
        if (d.y > b.y) d.y = b.y;
        if (d.z > b.z) d.z = b.z;
        return d;
    }

    // Return a vector whos' components are mixed to produce the highest values.
    inline Vector vcompmax(const Vector& a, const Vector& b) {
        Vector d = a;
        if (d.x < b.x) d.x = b.x;
        if (d.y < b.y) d.y = b.y;
        if (d.z < b.z) d.z = b.z;
        return d;
    }

    // Limits *value* lower-bounded to *min*.
    template <typename T> inline T limit_min(const T& value, const T& min) {
        return (value < min ? min : value);
    }

    // Limits *value* upper-bounded to *min*.
    template <typename T> inline T limit_max(const T& value, T& max) {
        return (value > max ? max : value);
    }

    // Limits *value* lower-bounded to *min* and upper-bounded to *max*.
    template <typename T> inline T limit(const T& min, const T& value, const T& max) {
        return (value < min ? min : (value > max ? max : value));
    }

    // Chooses the minimum value.
    template <typename T> inline T min(const T& a, const T& b) {
        return ((a) < (b) ? (a) : (b));
    }

    // Chooses the maxmimum value.
    template <typename T> inline T max(const T& a, const T& b) {
        return ((a) > (b) ? (a) : (b));
    }

    // Computes a how many threads should be used to process the
    // estimated number of iterations *itercount* taking
    // *max_per_thread* into account.
    inline Int32 num_threads(Int32 itercount, Int32 max_per_thread, Int32 reduce = 0) {
        Int32 cpu_count = std::thread::hardware_concurrency();
        Int32 num_threads = (itercount + max_per_thread - 1) / max_per_thread;
        Int32 count = min<Int32>(cpu_count, num_threads);
        if (count - reduce >= 1)
            count -= reduce;
        return count;
    }

    // Returns the maximum number of iterations processed by a single
    // thread. return-value * thread_count may be larger than the
    // actual number of iterations passed. This must be taken into account
    // when assigning a slice to a thread!!
    inline Int32 slice_count(Int32 itercount, Int32 thread_count) {
        return (itercount + thread_count - 1) / thread_count;
    }

    // Returns the id of a DescID as Int32. Returns -1 on failure.
    inline Int32 get_descid_id(DescID& id) {
        Int32 depth = id.GetDepth();
        if (depth <= 0) return -1;
        else return id[depth - 1].id;
    }

    // Returns a sub-contain of the passed container which defines an entry
    // MENURESOURCE_SUBTITLE and only if it equals the passed string.
    BaseContainer* find_submenu(BaseContainer* menu, const String& ident);

    // Loads a BaseBitmap from the plugins resoure folder. Like AutoBitmap,
    // but does not free the bitmap automatically.
    inline BaseBitmap* resource_bitmap(const String& name) {
        Filename path = GeGetPluginPath();
        path = path + "res" + name;
        BaseBitmap* bmp = BaseBitmap::Alloc();
        if (bmp && bmp->Init(path) != IMAGERESULT_OK) {
            BaseBitmap::Free(bmp);
        }
        return bmp;
    }

    // Alternative to the Cinema 4D dependence list.
    class DependenceList {

        Int32 m_count;

    public:

        DependenceList() : m_count(-1) { }

        /**
         * Return true if the list has changed, False if not. The target
         * list is updated.
         */
        Bool Compare(DependenceList& compareAgainst) const {
            if (compareAgainst.m_count != m_count) {
                compareAgainst.m_count = m_count;
                return true;
            }
            return false;
        }

        /**
         * Adds the object *op* to the dependence list.
         */
        void Add(BaseObject* op, DIRTYFLAGS flags=DIRTYFLAGS_ALL) {
            if (m_count < 0)
                m_count = 0;
            m_count += op->GetDirty(flags);
        }

    };

} // end namespace helpers
} // end namespace pr1mitive

#endif // PR1MITIVE_HELPERS_H
