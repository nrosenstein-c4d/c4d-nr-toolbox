// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef PR1MITIVE_DEFINES_H
#define PR1MITIVE_DEFINES_H

    #include <c4d_apibridge.h>
    using c4d_apibridge::IsEmpty;

    #ifndef PR1MITIVE_EXTERNAL_INCLUDE
        #include "misc/raii.h"
        #include "res/c4d_symbols.h"
    #endif // PR1MITIVE_EXTERNAL_INCLUDE

    #define PR1MITIVE_ICON(name) raii::AutoBitmap("icons/" name ".png")

#endif // PR1MITIVE_DEFINES_H
