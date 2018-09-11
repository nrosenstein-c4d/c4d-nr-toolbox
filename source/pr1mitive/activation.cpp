// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/debug.h>
#include "pr1mitive_serial.h"
#include <pr1mitive/helpers.h>
#include <pr1mitive/objects/BasePrimitiveData.h>
#include <pr1mitive/activation.h>
#include "menu.h"

namespace pr1mitive {

    namespace objects {
        extern Bool register_room();
        extern Bool register_proxy_generator();
    }

    namespace splines {
        extern Bool register_complexspline_base();
        extern Bool register_torusknot();
        extern Bool register_randomwalk();
        extern Bool register_splinechamfer();
    }

    namespace shapes {
        extern Bool register_complexshape_base();
        extern Bool register_pillow();
        extern Bool register_seashell();
        extern Bool register_jet_shape();
        extern Bool register_umbilictorus();
        extern Bool register_spintorus();
        extern Bool register_teardrop();
        extern Bool register_expression_shape();
    }

    namespace tags {
        extern Bool register_axisalign_tag();
    }

    namespace activation {

        Bool activation_start() {
            // Bases
            objects::BasePrimitiveData::static_init();
            splines::register_complexspline_base();
            shapes::register_complexshape_base();

            // Objects
            objects::register_room();
            objects::register_proxy_generator();
            menu::root().AddSeparator(IDS_MENU_OBJECTS);
            shapes::register_jet_shape();
            shapes::register_seashell();
            shapes::register_pillow();
            shapes::register_umbilictorus();
            shapes::register_spintorus();
            shapes::register_teardrop();
            shapes::register_expression_shape();

            // Splines
            splines::register_torusknot();
            splines::register_splinechamfer();
            splines::register_randomwalk();

            // Tags
            tags::register_axisalign_tag();
            return true;
        }

        Bool activation_msg(Int32 type, void* ptr) {
            return true;
        }

        void activation_end() {
            objects::BasePrimitiveData::static_deinit();
        }

    }; // end namespace activation
}; // end namespace pr1mitive

