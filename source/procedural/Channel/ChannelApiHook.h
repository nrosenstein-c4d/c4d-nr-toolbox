/// Copyright (C) 2015, Niklas Rosenstein
/// All rights reserved.
///
/// \file procedural/Channel/ChannelApiHook.h
/// \lastmodified 2015/06/01
/// \desc This file re-implements some Cinema 4D API functions to make them
///     function differently than they would usually do for the Procedural
///     Channel plugin.

#ifndef PROCEDURAL_INTERNAL_CHANNEL_CHANNELAPIHOOK_H_
#define PROCEDURAL_INTERNAL_CHANNEL_CHANNELAPIHOOK_H_

#include <c4d.h>

namespace nr {
namespace procedural {

  /// ************************************************************************
  /// This class provides some methods that will be hooked into the Cinema
  /// 4D API to function differently for the Procedural Channel plugin than
  /// they would normally do.
  /// ************************************************************************
  class ChannelApiHook
  {
  public:
    /// This hooked API call ensures that, for a Channel Node, a DescID with \var
    /// PROCEDURAL_CHANNEL_PARAM_DATA as the first ID will be routed \em directly
    /// to the underlying \ref NodeData. We're not entirely sure if there isn't
    /// anything else that happens in the original GetParameter() function.
    Bool GetParameter(const DescID& id, GeData& data, DESCFLAGS_GET flags);

    /// This hooked API call ensures that, for a Channel Node, a DescID with \var
    /// PROCEDURAL_CHANNEL_PARAM_DATA as the first ID will be routed \em directly
    /// to the underlying \ref NodeData and that \c SetDirty(DIRTYFLAGS_DATA)
    /// will \em not be called on the node for this parameter!
    Bool SetParameter(const DescID& id, const GeData& data, DESCFLAGS_SET flags);

    static Bool Register();
  private:
    static decltype(C4DOS.At->GetParameter) s_GetParameter;
    static decltype(C4DOS.At->SetParameter) s_SetParameter;
  };


} // namespace procedural
} // namespace nr

#endif // PROCEDURAL_INTERNAL_CHANNEL_CHANNELAPIHOOK_H_
