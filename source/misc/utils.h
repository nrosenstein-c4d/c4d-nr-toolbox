// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.
/*!
 * @file misc/utils.h
 * @description General utility functions for Cinema 4D.
 */

#pragma once
#include <c4d.h>
#include <nr/c4d/functional.h>

namespace utils {

//============================================================================
/*!
 * Move the points of a #PointObject to the axis so they appear
 * centered. Returns a matrix with only the #Matrix::off component
 * filled that can be multiplied with the object's axis to move
 * the object so its points appear in their original world location.
 *
 * @return The matrix to multiply with the object's matrix to
 *   make the points appear back in their original world location.
 */
//============================================================================
inline Matrix CenterAxis(PointObject* op) {
  Vector* points = op->GetPointW();
  Int32 const pcnt = op->GetPointCount();
  if (!points || pcnt == 0) return Matrix();

  MinMax mm;
  for (Int32 i = 0; i < pcnt; ++i)
    mm.AddPoint(points[i]);

  Vector mid = mm.GetMp();
  for (Int32 i = 0; i < pcnt; ++i) {
    points[i] -= mid;
  }
  op->Message(MSG_UPDATE);

  Matrix res;
  res.off = mid;
  return res;
}

//============================================================================
/*!
 * @param op The #BaseObject to read the #Tphong #BaseTag from.
 * @return The phong angle, 0.0 if there is no phong tag or
 *   #PI (180 degrees) if the limit checkbox is off.
 */
//============================================================================
inline Float GetPhongAngle(BaseObject* op) {
  BaseTag* tag = op->GetTag(Tphong);
  if (tag) {
    GeData limit;
    tag->GetParameter(PHONGTAG_PHONG_ANGLELIMIT, limit, DESCFLAGS_GET_0);
    if (!limit.GetBool()) return PI;  // 180 deg
    GeData angle;
    tag->GetParameter(PHONGTAG_PHONG_ANGLE, angle, DESCFLAGS_GET_0);
    return angle.GetFloat();
  }
  return 0.0;
}

//============================================================================
/*!
 * Given a node pointer with the #BaseList2D interface, this function
 * returns the node that is next in the hierarchy when traversing it
 * top-down.
 */
//============================================================================
template <typename T>
inline T* GetNextNode(T* node) {
  if (node->GetDown()) return node->GetDown();
  while (node->GetUp() && !node->GetNext())
    node = node->GetUp();
  return node->GetNext();
}

//============================================================================
/*!
 * This simple function calls #BaseThread::TestBreak() if the specified
 * #bt is not nullptr, but only every other #INDEX (which can be specified
 * via a template parameter). This is to reduce the overhead of calling
 * #BaseThread::TestBreak().
 */
//============================================================================
template <int INDEX>
inline Bool TestBreak(Int32 i, BaseThread* bt) {
  if (bt && i % INDEX == 0) return bt->TestBreak();
  return false;
}

//============================================================================
/*!
 * Calls #BaseThread::TestBreak() and returns the result or returns false
 * if #bt is a nullptr.
 */
//============================================================================
inline Bool TestBreak(BaseThread* bt) {
  if (bt) return bt->TestBreak();
  return false;
}

//============================================================================
/*!
 * Helper for timing multiple sections of execution.
 */
//============================================================================
class Timer {
public:
  Float64 tstart;
  maxon::BaseArray<Float64> times;

  Timer() : tstart(GeGetMilliSeconds()), times() { }

  Float64 Checkpoint() {
    Float64 ctime = GeGetMilliSeconds();
    Float64 interval = ctime - this->tstart;
    this->times.Append(interval);
    this->tstart = ctime;
    return interval;
  }

  void Start() {
    this->tstart = GeGetMilliSeconds();
  }

  Float64 Sum(Bool current_interval=true) const {
    Float64 sum = 0.0;
    if (current_interval)
      sum = (GeGetMilliSeconds() - this->tstart);
    for (Float64 const& v : this->times)
      sum += v;
    return sum;
  }
};

} // namespace utils
