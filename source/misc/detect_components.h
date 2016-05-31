// Copyright (C) 2016  Niklas Rosenstein
// All rights reserved.

#pragma once
#include <c4d.h>

//============================================================================
/*!
 * @define HAVE_VERTEXCOLOR
 *
 * Cinema 4D R18 comes with a new "Vertex Color" tag. This macro is being
 * set when the #API_VERSION is >= 18000 (R18).
 */
//============================================================================
#if API_VERSION >= 18000
#define HAVE_VERTEXCOLOR
#endif
