/// Copyright (C) 2015, Niklas Rosenstein
/// All rights reserved.
///
/// \file procedural/Channel/Channel.h
/// \lastmodified 2015/05/29

#ifndef PROCEDURAL_INTERNAL_CHANNEL_CHANNEL_H_
#define PROCEDURAL_INTERNAL_CHANNEL_CHANNEL_H_

#include <c4d.h>

namespace nr {
namespace procedural {

  /// ************************************************************************
  /// Registers the Procedural Channel plugin.
  /// ************************************************************************
  Bool RegisterChannelPlugin();

  /// ************************************************************************
  /// Unload the Procedural Channel plugin.
  /// ************************************************************************
  void UnloadChannelPlugin();

} // namespace procedural
} // namespace nr

#endif // PROCEDURAL_INTERNAL_CHANNEL_CHANNEL_H_
