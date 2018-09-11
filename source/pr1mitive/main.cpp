// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/defines.h>
#include <pr1mitive/activation.h>
#include <pr1mitive/help.h>
#include <NiklasRosenstein/c4d/cleanup.hpp>

namespace nr { using namespace niklasrosenstein; }

Bool RegisterPr1mitive() {
    nr::c4d::cleanup([] {
        pr1mitive::activation::activation_end();
    });

    if (!pr1mitive::help::install_help_hook()) return false;
    if (!pr1mitive::activation::activation_start()) return false;
    return true;
}

Bool MessagePr1mitive(Int32 type, void* ptr) {
    return pr1mitive::activation::activation_msg(type, ptr);
}
