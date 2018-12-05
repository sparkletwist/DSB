/*

- Added prestartup.lua to allow custom code to run before the majority of system scripts run
- Added graphics_paths and sound_paths tables stored in graphics.cfg and sound.cfg to allow different paths for base graphics and sounds
- Kept internal condition bitmaps from being accidentally trashed by the Lua garbage collector
- Added support for a secondary image for wall windows that can be larger than the wall that contains them
- Added ability to specify static images for wallitems at different ranges
- Added dynamic_shade arch property to control whether floor/wallitems are dynamically shaded
- Added door_draw_info table to give more control over how doors are drawn
- Added extra properties to gui_info.guy_icons to control the size of the icons
- Added multidraw arch property to circumvent DM-style wallitem drawing
- Added LeaderVision ini option to rotate the view when the party leader rotates
- Modified most functions that play sounds to optionally take a table of sounds and choose from it randomly
- Passing a sound handle of -1 to dsb_stopsound will now stop all playing sounds
- An icon specified for inventory_info.exitbox is now actually drawn
- Changed clickable flooritems to never be restricted to one clickable item per tile
- Moved the list of ammo storing locations out of local variables in attack methods
- Added additional renderer hooks to make changes to graphics in custom dungeons easier
- Fixed a bug in the throwtrolin importable archetype
- Fixed some internal message queue stuff that should hopefully make tracking down future bugs easier

*/

// TODO: Better way to work on dsb data files
// TODO: Support for subtile monster movement(?)
// TODO: Load fonts with alpha channel support -- doesn't seem possible with current Allegro

// TODO: Problem with sys_ functions... some are good for overriding, others are bulky?

/*
TODO: Monsters can send a message on various events

TODO: Specify the delay for cumulative damage
TODO: Multiple enemy graphics possible without dsb_qswapping between archs
TODO: GUI stuff should respect transparency and alpha channels
TODO: Use DM icon sheet for ESB
TODO: Food/water goes down too fast maybe? (check with CSBwin code)

TODO: Support alpha masks on doors with decorations and such things
TODO: Wall window compo that supports alpha
TODO: Eventually remove all requirements to use 256 color icons/graphics
TODO: Get rid of FTL font in file requests
TODO: User bindable keys (TAB for custom GUI button)
TODO: Change size of inventory subrenderers (fix res/rei to make this easier)
TODO: Be able to move champions ppos around
TODO: Sound volume control

TODO: Add optional parameter to dsb_viewport_distort to better control the distortion
TODO: Expand the usefulness of setup_icons(gfx_table_entry, bitmap)
TODO: Maybe integrate gameloop into fullscreen renderer so game can tick
TODO: Ambient light arch based solely on radius
TODO: Specify "transparent?" for monsters
TODO: Add a creature naming scroll to the standard archetypes
TODO: Add flag to allow monsters to move through walls and be drawn
TODO: Recolorable graphics with alpha channels -- or something like that to use pixel-level tinting
TODO: Control over height of flying objects
TODO: Add alternate front view to wallset_ext
TODO: Set "default" attack method that you can get by right clicking


*/

/*
ESB todo:
metaobjects/wizards?
graphics assistant?
*/
