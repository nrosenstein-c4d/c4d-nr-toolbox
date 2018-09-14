# TimeHide

**TimeHide** was originally available as a separate plugin on the
[niklasrosenstein.com] online store and is now integrated into **NR Workflow
Tools**. It provides an enhanced workflow for animators that regularly deal
with large amounts of animated objects, removing the clutter from the Cinema
4D **Timeline** and showing you only what you want to focus on.

[niklasrosenstein.com]: https://niklasrosenstein.com

In the *nr-toolbox*, TimeHide can no longer be found in the Project Settings
but instead in a separate Attribute Manager Mode called "nr-toolbox".

![](timehide-params.png)

> __Original Plugin Description__
>
> Working with the Timeline in Cinema 4D can become uncomfortable very fast.
> Only a few objects and parameters animated can let you loose the overview.
> Cinema 4D’s “Automatic Mode ” and “Link View to Object Manager” commands do
> not help much.
>
> The TimeHide plugin helps you to keep your Timeline in Cinema 4D clean and
> organized. Unnecessary elements are hidden so you only see what’s important.
> he plugin adds a new tab to your document settings. From there, you can
> enable or disable the functionality of the plugin.
>
> *Features*
>
> - Keep focused on what you need to see in the Timeline!
> - Scripts to Toggle parameters On/Off included
> - Free Service Updates
>
> *Languages*
>
> - English
> - German
> - Czech (thanks to Lubomir Bezek)
> - French (thanks to Antoine Aurety)
> - Japanese (thanks to Toshihide Miyata)

#### Parameters

The **Only Show Selected Elements** option will hide all elements from the
Timeline that are not selected in their respective other manager (ie. the
Object Manager, Material Manager, XPresso Manager, etc.).

![](timehide-only-show-selected-elemenets.gif)

The **Only Show Animated Elements** option will hide all elements that are
currently displayed in the Timeline, but that do not have any keyframes set
(eg. if you deleted all keys of a track, the object would still be visible).

In the below GIF, you can see an example with the parameter enabled and also
with the *Tracks within the Preview Range* mode set in the **Display Tracks**
parameter.

![](timehide-only-show-animated-in-preview-range.gif)

#### Scripts

You can find two scripts in the plugin's `res/scripts/` directory that allow
you to quickly toggle the parameters of the TimeHide plugin by assinging
keyboard shortcuts to these scripts.

Copy the scripts into your Cinema 4D scripts folder and you will find them
in the Scripts menu and the Command Manager.
