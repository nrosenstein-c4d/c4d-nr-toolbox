/// Copyright (C) 2015, Niklas Rosenstein
/// All rights reserved.
///
/// \file procedural/Channel/ChannelApiHook.cpp
/// \lastmodified 2015/06/01

#include <nr/macros.h>
#include <nr/c4d/util.h>
#include <nr/procedural/channel.h>

#include "ChannelApiHook.h"
using nr::procedural::ChannelApiHook;

NR_DECLMEMBER(ChannelApiHook::s_GetParameter);
NR_DECLMEMBER(ChannelApiHook::s_SetParameter);

/// **************************************************************************
/// **************************************************************************
Bool ChannelApiHook::GetParameter(const DescID& id, GeData& data, DESCFLAGS_GET flags)
{
  C4DAtom* atom = reinterpret_cast<C4DAtom*>(this);
  if (atom->GetType() == PROCEDURAL_CHANNEL_ID && id[0].id == PROCEDURAL_CHANNEL_PARAM_DATA) {
    GeListNode* node = static_cast<GeListNode*>(atom);
    NodeData* nodeData = node->GetNodeData<NodeData>();
    NODEPLUGIN* table = nr::c4d::retrieve_table<NODEPLUGIN>(nodeData);
    if (nodeData && table) {
      return (nodeData->*table->GetDParameter)(node, id, data, flags);
    }
  }
  return (atom->*s_GetParameter)(id, data, flags);
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelApiHook::SetParameter(const DescID& id, const GeData& data, DESCFLAGS_SET flags)
{
  C4DAtom* atom = reinterpret_cast<C4DAtom*>(this);
  if (atom->GetType() == PROCEDURAL_CHANNEL_ID && id[0].id == PROCEDURAL_CHANNEL_PARAM_DATA) {
    GeListNode* node = static_cast<GeListNode*>(atom);
    NodeData* nodeData = node->GetNodeData<NodeData>();
    NODEPLUGIN* table = nr::c4d::retrieve_table<NODEPLUGIN>(nodeData);
    if (nodeData && table) {
      return (nodeData->*table->SetDParameter)(node, id, data, flags);
    }
  }
  return (atom->*s_SetParameter)(id, data, flags);
}

/// **************************************************************************
/// **************************************************************************
Bool ChannelApiHook::Register()
{
  s_GetParameter = C4DOS.At->GetParameter;
  s_SetParameter = C4DOS.At->SetParameter;
  C4DOS.At->GetParameter = (decltype(s_GetParameter)) &ChannelApiHook::GetParameter;
  C4DOS.At->SetParameter = (decltype(s_SetParameter)) &ChannelApiHook::SetParameter;
  return true;
}
