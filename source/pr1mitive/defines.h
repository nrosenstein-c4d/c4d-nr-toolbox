// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#define _USE_MATH_DEFINES
#include <math.h>

#ifndef PR1MITIVE_DEFINES_H
#define PR1MITIVE_DEFINES_H


    #ifndef PR1MITIVE_EXTERNAL_INCLUDE
        #include "misc/legacy.h"
        #include "misc/raii.h"
        #include "res/c4d_symbols.h"
    #endif // PR1MITIVE_EXTERNAL_INCLUDE

    #define PR1MITIVE_ICON(name) raii::AutoBitmap("icons/" name ".png")

    // Macros for readability of source-code.
    // --------------------------------------

    #ifndef is
    #define is ==
    #endif

    #ifndef isnot
    #define isnot !=
    #endif

    #ifdef _MSC_VER
        #ifndef not
        #define not !
        #endif

        #ifndef or
        #define or ||
        #endif

        #ifndef and
        #define and &&
        #endif
    #endif

    // Constants (true, false, null)
    // -----------------------------

    #ifndef null
    #define null (0L)
    #endif

#endif // PR1MITIVE_DEFINES_H
