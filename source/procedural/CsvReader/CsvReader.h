/// Copyright (C) 2015, Niklas Rosenstein
/// All rights reserved.
///
/// \file procedural/CsvReader/CsvReader.h
/// \lastmodified 2015/05/29

#ifndef PROCEDURAL_INTERNAL_CSVREADER_CSVREADER_H_
#define PROCEDURAL_INTERNAL_CSVREADER_CSVREADER_H_

#include <c4d.h>

namespace nr {
namespace procedural {

  /// ************************************************************************
  /// Registers the Procedural Channel plugin.
  /// ************************************************************************
  Bool RegisterCsvReaderPlugin();

  /// ************************************************************************
  /// Unload the Procedural Channel plugin.
  /// ************************************************************************
  void UnloadCsvReaderPlugin();

}
}

#endif // PROCEDURAL_INTERNAL_CSVREADER_CSVREADER_H_
