/// Copyright (C) 2015, Niklas Rosenstein
/// All rights reserved.
///
/// \file Main.cpp
/// \lastmodified 2015/05/29

#include <c4d.h>
#include "procedural/Channel/Channel.h"
#include "procedural/CsvReader/CsvReader.h"

#include <nr/c4d/cleanup.h>

using namespace nr::procedural;

Bool RegisterProcedural()
{
  nr::c4d::cleanup([] {
    nr::procedural::UnloadChannelPlugin();
    nr::procedural::UnloadCsvReaderPlugin();
  });
  if (!nr::procedural::RegisterChannelPlugin()) return false;
  if (!nr::procedural::RegisterCsvReaderPlugin()) return false;
  return true;
}
