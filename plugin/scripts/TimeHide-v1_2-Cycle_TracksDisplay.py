# Copyright (c) 2013, Niklas Rosenstein
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
# File: TimeHide-v1_1-Cycle_TracksDisplay.py
# Version: 1.2
# Description:
#
#     This script cycles through the valid choices in the "Tracks Display"
#     parameter of the TimeHide project settings. The TimeHide plugin must
#     be installed. Visit http://niklasrosenstein.de/

import c4d
import webbrowser

def main():
    if not hasattr(c4d, 'Htimehide'):
        message = "It seems like the TimeHide plugin is not installed. Do you want to "\
                  "visit the download page?"
        result = c4d.gui.MessageDialog(message, c4d.GEMB_YESNO)
        if result == c4d.GEMB_R_YES:
            webbrowser.open('http://niklasrosenstein.de/tag/timehide')
        return

    choices = [
        c4d.TIMEHIDE_TRACKS_SHOWALL,
        c4d.TIMEHIDE_TRACKS_ANIMATED,
        c4d.TIMEHIDE_TRACKS_PREVIEWRANGE,
    ]

    hook = doc.FindSceneHook(c4d.Htimehide)
    if hook:
        doc.AddUndo(c4d.UNDOTYPE_CHANGE_SMALL, hook)
        value = hook[c4d.TIMEHIDE_TRACKS]
        if value in choices:
            value = choices[((choices.index(value) + 1) % len(choices))]
        else:
            value = c4d.TIMEHIDE_TRACKS_SHOWALL

        hook[c4d.TIMEHIDE_TRACKS] = value
        c4d.EventAdd()

main()

