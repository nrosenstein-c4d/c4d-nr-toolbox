/// Copyright (C) 2015, Niklas Rosenstein
/// All rights reserved.
///
/// \file logging.h
/// \lastmodified 2016/01/23

#pragma once
#include <c4d.h>
#include <nr/logging.h>
#include <nr/c4d/raii.h>


//  --------------------------------------------------------------------------
/// <nr/logging.h> backend function.
inline void _nr_logging_backend(
  char const* filename,
  int line,
  char const* func,
  nr::logging::Level level,
  char const* message)
{
  GePrint(String("[") + nr::tostr(level) + "]: " + String(message));
}

//  --------------------------------------------------------------------------
/// String type conversion specialization for the @NR_LOG() macro.
template <>
class _nr_logging_stringraii<String>
{
  char const* m;
public:
  _nr_logging_stringraii(String const& s) : m(s.GetCStringCopy(STRINGENCODING_UTF8)) { }
  ~_nr_logging_stringraii() { DeleteMem(m); }
  operator char const* () { return m; }
};
