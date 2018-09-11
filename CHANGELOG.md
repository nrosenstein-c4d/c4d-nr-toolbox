c4d-nr-toolbox Changelog
========================

TimeHide
--------

Version 1.4  -  ??

- Now included in the nr-toolbox plugin collection
- TimeHide now really does nothing when it is disabled. Before it would
  still update the tracks in your scene, but with no actual change.
- Added notice to README about the Cinema 4D Timeline "View > Show >
  Show Animated" option which can cause flicker in combination with the
  TimeHide "Only Show Animated Elements" option

Version 1.3  -  2014-07-04

- Fixed display of Shaders in the Timeline. In former versions,
  Shaders were completely hidden when the "Only Show selected Elements"
  option was turned on. Now, they are displayed if the Material is selected.
- Removed debug-print that sneaked into the 1.2 release version.
- Added French translation (thanks to Antoine Aurety)
- Added Japanese translation (thanks to Toshihide Miyata)

Version 1.2  -  2013-12-31

- The checkbox "Only Show Animated Elements" has been added. It behaves
  very similar to the "Only Show Selected Elements" checkbox but instead
  of hiding all elements that are not selected, it results in hiding all
  elements that are not animated. This fixes the bug, that, when dragging
  unanimated objects into the TimeLine, they were hidden nevertheless.
- If the "Only Show Animated Elements" and "Only Show Selected Elements"
  options are turned on, the latter has the preference. This means, if
  an object is not animated is selected, it is displayed in the timeline
  nevertheless.
- The Motion System Layers are now visible (previously hidden by the
  TimeHide plugin) and only hidden when not selected and the "Only
  Show Selected Elements" option is enabled.
- Minor performance improvements

Version 1.1  -  2013-09-10

- The parameter "Only show animated Tracks in Preview Range" has been
  split into a Combobox with three possible values and is now call
  "Display Tracks".

  * All Tracks
  * Animated Tracks
  * Tracks within the Preview Range

- Performance improvements and small bug fixes.

Version 1.0  -  2013-08-04

- Initial version
