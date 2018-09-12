# Copyright (c) 2018, Niklas Rosenstein
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# File: TimeHide-v1_3-Toggle.py
# Version: 1.3
# Description:
#
#     This script toggles the "Only Show Selected Elements" and "Only Show
#     Animated Elements" in the nr-toolbox TimeHide settings.

import c4d
import webbrowser

def main():
    if not hasattr(c4d, 'Hnrtoolbox'):
        message = "It seems like the nr-toolbox plugin is not installed. Do you want to "\
                  "visit the download page?"
        result = c4d.gui.MessageDialog(message, c4d.GEMB_YESNO)
        if result == c4d.GEMB_R_YES:
            webbrowser.open('https://github.com/NiklasRosenstein/c4d-nr-toolbox')
        return

    hook = doc.FindSceneHook(c4d.Hnrtoolbox)
    if hook:
        doc.AddUndo(c4d.UNDOTYPE_CHANGE_SMALL, hook)
        hook[c4d.NRTOOLBOX_HOOK_TIMEHIDE_ONLYSELECTED] = not hook[c4d.NRTOOLBOX_HOOK_TIMEHIDE_ONLYSELECTED]
        hook[c4d.NRTOOLBOX_HOOK_TIMEHIDE_ONLYANIMATED] = not hook[c4d.NRTOOLBOX_HOOK_TIMEHIDE_ONLYANIMATED]
        c4d.EventAdd()
    else:
        message = "Unable to find the nr-toolbox settings in the current document."
        c4d.gui.MessageDialog(message)

main()
