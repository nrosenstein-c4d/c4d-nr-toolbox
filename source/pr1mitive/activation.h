// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

//#include <lib_sn.h>
#include <pr1mitive/defines.h>
#include <pr1mitive/helpers.h>

#ifndef ACTIVATION_H_INCLUDED
#define ACTIVATION_H_INCLUDED

namespace pr1mitive {
namespace activation {

    // Checks if the license correct and registers the plugins respectively.
    Bool activation_start();

    // Called when a plugin-message appears.
    Bool activation_msg(Int32 type, void* ptr);

    // Performs unloads, cleanups, etc.
    void activation_end();

    // Enhances the Cinema 4D Menu.
    void enhance_menu();

}; // end namespace activation
}; // end namespace pr1mitive

#endif // ACTIVATION_H_INCLUDED
