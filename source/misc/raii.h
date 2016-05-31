// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.

#pragma once
#include <c4d.h>
#include <nr/c4d/functional.h>
#include "print.h"

namespace raii {

//============================================================================
/*!
 * This class is like the Cinema 4D AutoBitmap class, but it supports listing
 * subdirectories in the constructor. With the C4D AutoBitmap class, you can
 * not load an icon from a subdirectory.
 *
 *     AutoBitmap("icons/myplugin.tif")  // does not work
 */
//============================================================================
class AutoBitmap {
private:
  Bool owner;
  BaseBitmap* bmp;
public:
  AutoBitmap(Filename fn) {
    owner = true;
    bmp = BaseBitmap::Alloc();
    if (!bmp) {
      return;
    }
    // The C4D Filename class is stupid, so we need to do string
    // concatenation to not loose all but the last part of the filename.
    fn = GeGetPluginResourcePath().GetString() + "/" + fn.GetString();
    IMAGERESULT result = bmp->Init(fn);
    if (result != IMAGERESULT_OK) {
      print::error("Failed to load image: " + fn.GetString());
      BaseBitmap::Free(bmp);
    }
  }
  ~AutoBitmap() {
    BaseBitmap::Free(bmp);
  }
  operator BaseBitmap* () const { return bmp; }
  BaseBitmap* operator -> () const { return bmp; }
  BaseBitmap* Release() { BaseBitmap* temp = bmp; bmp = nullptr; return temp; }
};

//============================================================================
/*!
 * RAII for calling `Free()` but with move semantics.
 */
//============================================================================
template <class T>
class AutoFree {
private:
  T* ptr;
public:
  AutoFree() : ptr(nullptr) { }
  AutoFree(T* ptr) : ptr(ptr) { }
  AutoFree(AutoFree const&) = delete;
  AutoFree(AutoFree&& other) : ptr(std::move(other.ptr)) { }
  ~AutoFree() { T::Free(ptr); }
  void Assign(T* new_ptr) { Free(); ptr = new_ptr; }
  void Free() { T::Free(ptr); }
  T* Release() { T* temp = ptr; ptr = nullptr; return temp; }
  T* operator -> () const { return ptr; }
  AutoFree& operator = (T* new_ptr) {
    if (ptr) T::Free(ptr);
    ptr = new_ptr;
    return *this;
  }
  operator T* () const { return ptr; }
};

//============================================================================
/*!
 * RAII helper to call a function when a scope exits.
 */
//============================================================================
class RAII {
private:
  std::function<void()> func;
public:
  RAII(std::function<void()> const& func) : func(func) { }
  RAII(RAII const&) = delete;
  RAII(RAII&&) = delete;
  ~RAII() { func(); }
};

} // namespace raii
