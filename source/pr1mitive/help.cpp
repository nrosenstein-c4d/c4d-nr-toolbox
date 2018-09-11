// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/help.h>

namespace pr1mitive {
namespace help {

    #if API_VERSION >= 20000
    #define CallHelpBrowser OpenHelpBrowser
    using HelpResult = void;
    #define HelpResultFalse
    #else
    using HelpResult = Bool;
    #define HelpResultFalse false
    #endif

    static HelpResult (*old_CallHelpBrowser)(
            const c4d_apibridge::String&,
            const c4d_apibridge::String&,
            const c4d_apibridge::String&,
            const c4d_apibridge::String&) = nullptr;

    HelpResult call_help_browser(
        const c4d_apibridge::String& optype,
        const c4d_apibridge::String& main,
        const c4d_apibridge::String& group,
        const c4d_apibridge::String& property)
    {
        if (String(optype).SubStr(1, 5).ToLower() == String("pr1m")) {
            // TODO: Find and display help files!
            GePrint("DEBUG: Pr1mitive Help File requested: " + String(property));
            return HelpResultFalse;
        }

        if (old_CallHelpBrowser) {
            return old_CallHelpBrowser(optype, main, group, property);
        }
        else {
            return HelpResultFalse;
        }
    }

    Bool install_help_hook() {
        old_CallHelpBrowser = C4DOS.Ge->CallHelpBrowser;
        C4DOS.Ge->CallHelpBrowser = call_help_browser;
        return true;
    }

} // end namespace help
} // end namespace pr1mitive