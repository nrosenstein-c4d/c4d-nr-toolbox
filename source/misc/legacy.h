#pragma once

#ifndef CHAR
#define CHAR Char
#endif

#ifndef LONG
#define LONG Int32
#endif

#ifndef LLONG
#define LLONG Int64
#endif

#ifndef VLONG
#define VLONG Int
#endif

#ifndef SReal
#define SReal Float32
#endif

#ifndef Real
#define Real Float
#endif

#ifndef SetLong
#define SetLong SetInt32
#endif

#ifndef GetLong
#define GetLong GetInt32
#endif

#ifndef SetReal
#define SetReal SetFloat
#endif

#ifndef GetReal
#define GetReal GetFloat
#endif

#ifndef gNew
#define gNew NewObjClear
#endif

#ifndef GeAlloc
#define GeAlloc NewMem
#endif

#ifndef GeFree
#define GeFree DeleteMem
#endif

#include <c4d.h>

#ifdef _MSC_VER
  #pragma warning(push)
  #pragma warning(disable:4265)
  #include <thread>
  #pragma warning(pop)
#else
  #include <thread>
#endif

namespace c4d_misc = maxon;
inline String LongToString(LONG x) { return String::IntToString(x); }
inline String RealToString(Real x) { return String::FloatToString(x); }
inline LONG GeGetCPUCount() { return std::thread::hardware_concurrency(); }
inline Matrix operator ! (Matrix const& other) { return ~other; }
inline Vector operator ^ (Vector const& a, Vector const& b) { return a * b; }
inline Real VectorAngle(Vector const& a, Vector const& b) { return GetAngle(a, b); }

static THREADMODE THREADMODE_SYNCHRONOUS = THREADMODE_DEPRECATED_SYNCHRONOUS;
