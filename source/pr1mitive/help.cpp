// coding: ansii
//
// Copyright (C) 2012-2013, Niklas Rosenstein
// All rights reserved.

#include <pr1mitive/help.h>

namespace pr1mitive {
namespace help {

    static Bool (*old_CallHelpBrowser)(
            const String&, const String&,
            const String&, const String&) = nullptr;

    Bool call_help_browser(const String&, const String&, const String&, const String&);

    Bool install_help_hook() {
        old_CallHelpBrowser = C4DOS.Ge->CallHelpBrowser;
        C4DOS.Ge->CallHelpBrowser = call_help_browser;
        return TRUE;
    }

    Bool call_help_browser(
            const String& optype, const String& main,
            const String& group, const String& property) {

        if (optype.SubStr(1, 5).ToLower() == String("pr1m")) {
            // TODO: Find and display help files!
            GePrint("DEBUG: Pr1mitive Help File requested: " + property);
            return FALSE;
        }

        if (old_CallHelpBrowser) {
            return old_CallHelpBrowser(optype, main, group, property);
        }
        else {
            return FALSE;
        }
    }

} // end namespace help
} // end namespace pr1mitive