// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/defines.h>

#ifndef PR1MITIVE_DEBUG_H
#define PR1MITIVE_DEBUG_H

namespace pr1mitive {
namespace debug {

    #ifndef PR1MITIVE_DEBUG_CALL
    #define PR1MITIVE_DEBUG_CALL(x) GeDebugOut(x)
    #endif

    extern char _debug_buffer[];
    void _debug_message(const char* prefix, const char* funcname, const char* filename, int lineno, const char* message);
    void _debug_message(const char* prefix, const char* funcname, const char* filename, int lineno, String message);
    void _debug_printf(const char* format, ...);

    #ifdef DEBUG
        #define PR1MITIVE_DEBUG(message) \
            pr1mitive::debug::_debug_message(NULL, __FUNCTION__, __FILE__, __LINE__, message)
        #define PR1MITIVE_DEBUG_INFO(message) \
            pr1mitive::debug::_debug_message("INFO", __FUNCTION__, __FILE__, __LINE__, message)
        #define PR1MITIVE_DEBUG_WARNING(message) \
            pr1mitive::debug::_debug_message("WARNING", __FUNCTION__, __FILE__, __LINE__, message)
    #else
        #define PR1MITIVE_DEBUG(message)
        #define PR1MITIVE_DEBUG_INFO(message)
        #define PR1MITIVE_DEBUG_WARNING(message)
    #endif // DEBUG

    #if defined(DEBUG) || !defined(PR1MITIVE_DEBUG_NODEBUGNOERROR)
        #define PR1MITIVE_DEBUG_ERROR(message) \
            pr1mitive::debug::_debug_message("ERROR", __FUNCTION__, __FILE__, __LINE__, message)
    #endif

} // end namespace debug
} // end namespace pr1mitive

#endif // PR1MITIVE_DEBUG_H
