// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.
/*!
 * @file misc/print.h
 * @description Provides the #print namespace with useful fucntion for string
 *   formatting and printing to the Cinema 4D command-line.
 */

#pragma once
#include <c4d.h>

namespace print {

//============================================================================
//============================================================================
enum {
  PRINT_DEBUG = 1,
  PRINT_INFO = 2,
  PRINT_WARN = 3,
  PRINT_ERROR = 4,
};

//============================================================================
/*!
 * @define PRINT_LEVEL
 *
 * The level of prints that are shown. Any print statements with values
 * equal or above are printed, others are ignored. Defaults to #PRINT_INFO.
 */
//============================================================================
#ifndef PRINT_LEVEL
#define PRINT_LEVEL PRINT_INFO
#endif

//============================================================================
/*!
 * @define PRINT_PREFIX
 *
 * Prefix that is printed with every line of output. Defaults to an
 * empty string.
 */
//============================================================================
#ifndef PRINT_PREFIX
#define PRINT_PREFIX ""
#endif

//============================================================================
//============================================================================
// @{
inline String tostr(Int32 v) { return String::IntToString(v); }
inline String tostr(Int64 v) { return String::IntToString(v); }
inline String tostr(UInt32 v) { return String::UIntToString(v); }
inline String tostr(UInt64 v) { return String::UIntToString(v); }
inline String tostr(Float32 v) { return String::FloatToString(v); }
inline String tostr(Float64 v) { return String::FloatToString(v); }
inline String tostr(char const* c) { return String(c); }
inline String tostr(Vector const& v) { return String::VectorToString(v); }
inline String tostr(String const& s) { return s; }
//@}

//============================================================================
//============================================================================
//@{
template <typename T>
inline String _join_for_print(String const& sep, T const& t) {
  return tostr(t);
}

template <typename T, typename... R>
inline String _join_for_print(String const& sep, T const& t, R&&... r) {
  return tostr(t) + sep + _join_for_print(sep, std::forward<R>(r)...);
}
//@}

//============================================================================
/*!
 * A print function that supports variadic template arguments. All
 * parameters must be convertible to a string with the #tostr() function.
 */
//============================================================================
template <typename... R>
static inline void print(R&&... r) {
  GePrint(String(PRINT_PREFIX) + _join_for_print(String(" "), std::forward<R>(r)...));
}

//============================================================================
//============================================================================
template <typename... R>
static inline void info(R&&... r) {
  if (PRINT_LEVEL <= PRINT_INFO)
    print(std::forward<R>(r)...);
}

//============================================================================
//============================================================================
template <typename... R>
static inline void debug(R&&... r) {
  if (PRINT_LEVEL <= PRINT_DEBUG)
    print(std::forward<R>(r)...);
}

//============================================================================
//============================================================================
template <typename... R>
static inline void warn(R&&... r) {
  if (PRINT_LEVEL <= PRINT_WARN)
    print(std::forward<R>(r)...);
}

//============================================================================
//============================================================================
template <typename... R>
static inline void error(R&&... r) {
  if (PRINT_LEVEL <= PRINT_ERROR)
    print(std::forward<R>(r)...);
}

} // namespace print
