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

        #define STATUS_WRONG (1<<0)
        #define STATUS_DEMO  (1<<1)
        #define STATUS_OK    (1<<2)

        // Returns the string "DEMO" from an encrypted resource.
        #define get_demo_string ydosp_sfa992Msd__aefsdk__Adaf_adfPP
        static void get_demo_string(char* dest) {
            static const char* salt = "sf0\3sf\2\4.'d*";
            static const char* encrypted = "(91O";

            int j=0;
            for (int i=0; i < 4; i++) {
                dest[i] = encrypted[i] + (salt[j] / 4);
                j += 2;
            }
        }

        // Serial Hook for the registration of the Pr1mitive plugin within the Cinema 4D
        // registration dialog.
        class Pr1mSerialHook : public SNHookClass {

          private:

            static const LONG           id = 1028979;
            static Pr1mSerialHook*      instance;
            static LONG                 status;

          public:

          //
          // Pr1mSerialHook ---------------------------------------------------------------------------
          //

            // Returns the registration status code.
            static LONG get_status();

            // Registers the hook to Cinema 4D.
            static Bool enable_hook();

            // Frees allocated memory, etc.
            static void free_hook();

          //
          // SNHook -----------------------------------------------------------------------------------
          //

            LONG SNCheck(const String& c4d_serial, const String& serial, LONG regdate, LONG currdate);

            const String& GetTitle();

        };

        Pr1mSerialHook* Pr1mSerialHook::instance = null;
        LONG Pr1mSerialHook::status = STATUS_WRONG;

        LONG Pr1mSerialHook::get_status() {
            return status;
        }

        Bool Pr1mSerialHook::enable_hook() {
            if (not instance) {
                instance = new Pr1mSerialHook;
                if (not instance) {
                    return false;
                }
                 instance->Register(Pr1mSerialHook::id, SNFLAG_OWN);
            }
            return true;
        }

        void Pr1mSerialHook::free_hook() {
            if (instance) delete instance;
        }

        LONG Pr1mSerialHook::SNCheck(const String& c4dsn, const String& sn, LONG regdate, LONG currdate) {
            status = STATUS_WRONG;
            if (!regdate && !sn.Content()) return SN_WRONGNUMBER;

            // Allow NET Render.
            if (IsNet()) {
                status = STATUS_DEMO | STATUS_OK;
                return SN_NET;
            }

            // Demo Mode Activation
            char _demo[5] = {0};
            get_demo_string(_demo);
            String demo(_demo);
            if (sn == demo) {
                status = STATUS_DEMO;
                return SN_OKAY;
            }

            // Cinema 4D Activation License
            else {
                SerialInfo sninfo;
                GeGetSerialInfo(SERIALINFO_MULTILICENSE, &sninfo);

                String c4dlicense;

                // Check if we're in a multi-license environment and adjust the serial-number.
                if (sninfo.nr.Content()) c4dlicense = sninfo.nr;
                else c4dlicense = c4dsn;

                char* c4dlicense_raw = c4dlicense.GetCStringCopy();
                if (!c4dlicense_raw) {
                    PR1MITIVE_DEBUG_ERROR("Could not obtain C-String Copy.");
                    return SN_WRONGNUMBER;
                }

                serial::C4DLicense c4dlicense_obj(c4dlicense_raw);
                GeFree(c4dlicense_raw);

                if (c4dlicense_obj.type == serial::LICENSETYPE_INVALID) {
                    PR1MITIVE_DEBUG_ERROR("Cinema 4D license is invalid. Kind'a werid, because it is passed by C4D..");
                    return SN_WRONGNUMBER;
                }

                int length;
                char compare_serial[serial::SERIALLENGTH_UNIFIED + 1] = { 0 };
                serial::SERIALRESULT result = c4dlicense_obj.convert_unified(compare_serial, &length);
                if (result != serial::SERIALRESULT_OK) {
                    PR1MITIVE_DEBUG_ERROR("Comparable Serial could not be generated.");
                    return SN_WRONGNUMBER;
                }

                compare_serial[length] = 0;
                String original(compare_serial);
                if (original == sn) {
                    status = STATUS_DEMO | STATUS_OK;
                    return SN_OKAY;
                }
            }

            status = STATUS_WRONG;
            return SN_WRONGNUMBER;
        }

        const String& Pr1mSerialHook::GetTitle() {
            static String* title = null;
            if (not title) {
                title = new String(GeLoadString(IDS_PR1MITIVE_NAME, PR1MITIVE_VERSION_STRING));
            }
            return *title;
        }


        Bool activation_start() {
            LONG status = Pr1mSerialHook::get_status();
            if (!(status & STATUS_OK)) return true;

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

        Bool activation_msg(LONG type, void* ptr) {
            switch (type) {
                case C4DPL_INIT_SYS:
                    return Pr1mSerialHook::enable_hook();
            }
            return true;
        }

        void activation_end() {
            objects::BasePrimitiveData::static_deinit();
        }

    }; // end namespace activation
}; // end namespace pr1mitive

