/* Copyright (C) 2013-2016  Niklas Rosenstein
 * All rights reserved. */

#pragma once
#include <c4d.h>

enum
{
  // Sent to the hook that calls #THMessage() when #EVMSG_CHANGE
  // is received by a #MessageData plugin.
  MSG_TIMEHIDE_EVMSG_CHANGE = 1030804,
};

struct THStats;

THStats* THMessage(THStats* stats, BaseSceneHook* hook, BaseDocument* doc, Int32 msg, void* data);

void THFreeStats(THStats*& stats);
