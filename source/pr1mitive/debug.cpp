// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/debug.h>

namespace pr1mitive {
namespace debug {

    char _debug_buffer[512];

    void _debug_message(const char* prefix, const char* funcname, const char* filename, int lineno, const char* message) {
        if (prefix)
            sprintf(_debug_buffer, "%s: %s() in %s at line %d: %s", prefix, funcname, filename, lineno, message);
        else
            sprintf(_debug_buffer, "%s() in %s at line %d: %s", funcname, filename, lineno, message);
        PR1MITIVE_DEBUG_CALL(_debug_buffer);
    }

    void _debug_message(const char* prefix, const char* funcname, const char* filename, int lineno, String message) {
        char* string = message.GetCStringCopy();
        _debug_message(prefix, funcname, filename, lineno, string);
        GeFree(string);
    }

    void _debug_printf(const char* format, ...) {
        va_list vl;
        va_start(vl, format);
        vsprintf(_debug_buffer, format, vl);
        PR1MITIVE_DEBUG_CALL(_debug_buffer);
        va_end(vl);
    }

} // end namespace debug
} // end namespace pr1m

