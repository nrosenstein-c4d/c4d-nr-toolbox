/* Copyright (C) 2015, Niklas Rosenstein
 * All rights reserved.
 *
 * @file nr/bmp.h
 * @desc This header provides the template class DynamicArray which
 *   represents a resizable memory block that can be resized dynamically
 *   for an arbitrary datatype during runtime.
 */

#ifndef NR_DATATYPES_DYNAMICARRAY_H_
#define NR_DATATYPES_DYNAMICARRAY_H_

#include <c4d.h>
#include <nr/c4d/string.h>

namespace nr {
namespace procedural {
namespace dynamicarray {

/// **************************************************************************
/// This is an interface that must implement the allocation policy for a
/// DynamicArray. It can decide how much memory to allocate for a
/// specified amount of required memory (eg. to allocate memory in advance).
/// **************************************************************************
class AllocationPolicy
{
public:
  /// From the specified minimum number of bytes to allocate, compute a new,
  /// equal or bigger number of bytes to allocate instead.
  static UInt GetAllocSize(UInt minSize, UInt prevSize);
};

/// **************************************************************************
/// This implementation of the AllocationPolicy interface clips the
/// minimum allocation size to a boundary specified via \tparam BOUNDARY . As
/// an example, assume \c BOUNDARY=1024. If 200 bytes are requested, 1024
/// will be returned. If 1050 bytes are requested, 2048 will be returned, etc.
/// However, if a minimum size less than the original size is requested, a size
/// less than before will only be returned if its half or less the original
/// size.
/// **************************************************************************
template <UInt BOUNDARY>
class DefaultAllocationPolicy
{
public:
  /// @Override
  inline static UInt GetAllocSize(UInt minSize, UInt prevSize)
  {
    // Do not release any memory unless the minSize is less or equal
    // than half of the original memory.
    if (minSize < prevSize && minSize > prevSize / 2)
      minSize = prevSize;

    // Clip minSize to the specified boundary.
    UInt flood = minSize % BOUNDARY;
    if (flood != 0)
      minSize += BOUNDARY - flood;
    return minSize;
  }
};

/// **************************************************************************
/// The DynamicArray is a container class for a datatype generally
/// undetermined at compile-time and it must be tracked at run-time. The
/// \c DynamicArray makes sure that at least enough memory is allocated for
/// the required type and array size and calls default constructors and
/// destructors.
///
/// The template methods \fn Resize() and \fn Destroy() must be used to
/// specify the type for the array and the size. \fn Resize() can only be
/// called with the same type again or with a new type after \fn Destroy().
///
/// An implementation class of the AllocationPolicy interface can be
/// specified for the \tparam ALLOC_POLICY template parameter to specify the
/// way memory is allocated in advance.
/// **************************************************************************
template <typename ALLOC_POLICY=DefaultAllocationPolicy<2048>>
class DynamicArray
{
public:

  /// Default constructor.
  DynamicArray() : mTypeSize(0), mCount(0), mAllocSize(0), mData(nullptr)
  {
  }

  /// Destructor.
  ~DynamicArray()
  {
    /// Make sure the objects in the array have been destroyed.
    CriticalAssert(mTypeSize == 0);
    DeleteMem(mData);
    mCount = 0;
    mAllocSize = 0;
    mData = nullptr;
  }

  /// Resize the DynamicArray for type \tparam T . Can only be called
  /// again with the same type, if the array was destroyed with \fn Destory()
  /// or if it was just constructor.
  /// @return \c true if resize was successful, \c false if not.
  template <typename T>
  bool Resize(UInt count, const T& initValue)
  {
    // Make sure the array is empty or the type size matches (to have some
    // security at least).
    CriticalAssert(mTypeSize == 0 || mTypeSize == sizeof(T));

    // Call the destructor for all objects outside the scope.
    for (UInt index = count; index < mCount; ++index)
      reinterpret_cast<T*>(mData)[index].~T();

    // Determine the new memory size and reallocate it. We want to allocate
    // at least some bytes so we say that we allocate memory for at least
    // one element.
    UInt minSize = Max<UInt>(count, 1) * sizeof(T);
    UInt realSize = mPolicy.GetAllocSize(minSize, mAllocSize);
    if (realSize < minSize) {
      CriticalOutput("WARNING: mPolicy.GetAllocSize() realSize < minSize");
      realSize = minSize;
    }

    char* data = reinterpret_cast<char*>(mData);
    char* newData = ReallocMem(data, realSize);
    if (newData == nullptr) {
      // If we were only to shrink the memory, there sould be no error in
      // reallocating it. If there is, we already destroyed objects that
      // were valid in the original array and we prefer to not completely
      // deallocate the memory in the DynamicArray even if reallocation
      // failed.
      CriticalAssert(realSize > mAllocSize && count > mCount);
      return false;
    }

    // Call the constructors for all newly allocated elements.
    for (UInt index = mCount; index < count; ++index) {
      T* obj = reinterpret_cast<T*>(newData) + index;
      new (obj) T(initValue);
    }

    mTypeSize = sizeof(T);
    mData = reinterpret_cast<void*>(newData);
    mCount = count;
    mAllocSize = realSize;
    return true;
  }

  /// Destroy all elements in the DynamicArray of type \tparam T .
  /// Resets the array size to zero although it will not actually deallocate
  /// memory.
  template <typename T>
  void Destroy()
  {
    CriticalAssert(mTypeSize == 0 || mTypeSize == sizeof(T));
    for (UInt index = 0; index < mCount; ++index)
      reinterpret_cast<T*>(mData)[index].~T();
    mTypeSize = 0;
    mCount = 0;
    memset(mData, 0, mAllocSize);
  }

  /// @return \c true if the \p index is accessible in the array,
  ///     otherwise \c false otherwise.
  inline bool Accessible(UInt index) const
  {
    return mData && index < mCount;
  }

  /// @return \c true if the array is empty (destroyed), \c false if not.
  inline bool IsEmpty() const
  {
    return mTypeSize == 0;
  }

  /// @return the number of elements in the array.
  inline UInt GetCount() const
  {
    return mCount;
  }

  /// @return the type size of the array.
  inline UInt GetTypeSize() const
  {
    return mTypeSize;
  }

  /// @return the number of bytes currently allocated.
  inline UInt GetAllocSize() const
  {
    return mAllocSize;
  }

  /// @return the handle to the contiguous memory location.
  inline void* GetDataHandle() const
  {
    return mData;
  }

  /// @return the handle to the contiguos memory location, cast to
  ///     the specified type.
  template <typename T>
  T* Get() const
  {
    if (mTypeSize != 0) {
      CriticalAssert(mTypeSize == sizeof(T));
      return reinterpret_cast<T*>(mData);
    }
    return nullptr;
  }

  /// @return A string representation of the DynamicArray that
  ///     contains useful information about it.
  inline String ToString() const
  {
    using nr::c4d::tostr;
    return \
      "DynamicArray(mTypeSize=" + tostr(mTypeSize) + ", " +
      "mCount=" + tostr(mCount) + ", " +
      "mAllocSize=" + tostr(mAllocSize) + ", " +
      "mData=0x" + tostr(reinterpret_cast<UInt>(mData)) + ")";
  }

  /// @return A reference to the AllocationPolicy object that was
  ///     created from the template parameter #ALLOC_POLICY .
  ALLOC_POLICY& GetAllocPolicy()
  {
    return mPolicy;
  }

  /// @return A const reference to the AllocationPolicy object that
  ///     was created from the template parameter #ALLOC_POLICY .
  const ALLOC_POLICY& GetAllocPolicy() const
  {
    return mPolicy;
  }

private:
  ALLOC_POLICY mPolicy; ///< The AllocationPolicy implementation.
  UInt mTypeSize;       ///< The size of the type that is currently used.
  UInt mCount;          ///< The anmount of elements in the array.
  UInt mAllocSize;      ///< The anmount of memory in \var mData.
  void*  mData;         ///< The memory fo \var mAllocSize bytes.
};

} // end namespace dynamicarray
} // end namespace procedural
} // end namespace nr

#endif // NR_DATATYPES_DYNAMICARRAY_H_
