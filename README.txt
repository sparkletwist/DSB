*** DUNGEON STRIKES BACK ***

Dungeon Strikes Back (henceforth DSB) is a heavily Lua-powered and quite scriptable
attempt at a Dungeon Master-ish game engine. Compared to other engines, it is a bit
spartan at the present time, but it is also an early version and should hopefully grow--
perhaps more easily due to its large customization abilities.

If you just want to play a DSB dungeon, simply run DSB. You'll be prompted to choose
the directory holding the dungeon you want to play. Alternatively, edit dsb.ini,
set "Dungeon=" to the game directory of the dungeon you want to play, and run
DSB.exe. For example, to play the dungeon stored in the "mydungeon" folder,
change the line in dsb.ini to "Dungeon=mydungeon", save dsb.ini, and double-click
DSB.exe.

***

DSB's controls work basically like Dungeon Master. You can move around by clicking
the arrows, but you'll find it's probably easier and more intuitive to use the
number pad.

Keystrokes function by default like CSB: 8 for forward, 4 for left, and so on. If you
want to change them, you can also access this via the DSB Menu. Alternatively, to 
set it upon startup, set "Config=1" under "[Keyboard] in dsb.ini to configure the
keyboard. For the num pad, num lock is not necessary; the keys will always be handled
properly.

Most interactions in the game work via left-clicking. Right-clicking allows the
inventory screen to be quickly popped up. Middle-clicking, or holding down shift or ctrl
and left-clicking (or clicking the character's inventory bars) allows a held item
to be instantly put away. This can save a lot of fumbling around in the inventory.

Additional options can be selected by pausing the game (press ESC) and then pressing
enter to get to a DSB menu. This contains options that DSB offers that were not in
the original version of Dungeon Master.

To play in full screen mode, edit dsb.ini and set "Windowed=0" in the "[Settings]" section.
You can also switch between full screen and windowed mode in the DSB menu.

If DSB's window is too small, you can set "DoubleWindow=1" as well, and DSB will
play in a 1280x960 window instead of its default 640x480 one. All graphics will be
doubled in size. This mode is sometimes kind of slow, though.


***

A DSB dungeon, like so much else in DSB, is defined by a piece of Lua code. This
means that dungeons may be created by writing the appropriate Lua program. Lua programs
are simple text files that may be edited in any text editor. Notepad will suffice, though
something specifically designed for programming such as SciTE or LuaEdit may be better

DSB includes a graphical editor, called Editor Strikes Back, to help you in developing your
dungeon layouts. It won't allow you to do everything you need to do, but it will likely
make floorplans and other such tasks much easier.
 
To create a new dungeon, simply create a new folder and place files named "dungeon.lua"
and "startup.lua" into it. The dungeon's layout should be defined in "dungeon.lua," and
any additional data can be defined in "startup.lua." ESB can automatically create a
dungeon folder for you, as well. 

In startup.lua, you can declare a variable called lua_manifest. This will allow you
to specify other Lua files to load. See "global.lua" for an example.

Functions declared in your own dungeon's startup.lua (and files loaded from it) will
override default functions specified in the base files. This allows dungeon designers
to replace whatever default mechanics they'd like with their own.

If you don't know Lua, the first thing you should do if you want to create a DSB dungeon
is look at www.lua.org's documentation on the Lua language as well as the example Lua
sources included with DSB. Lua is a fairly simple but powerful language that resembles
Visual Basic or Java, and if you have any programming experience, it should come
fairly easily to you. Even if you don't, the syntax is rather intuitive.

To compile a dungeon you've created, change the line "Compile=0" in dsb.ini to
"Compile=1", and, optionally, add a "Dungeon=" line (if you want to work on the
same dungeon and don't want to bother with the selector every time); if you have any
custom graphics, this will also create a graphics.dsb. To distribute a dungeon that
you've made, all you have to distribute are your dungeon.dsb, and, if you have them,
graphics.dsb and/or any custom Lua that you've written.


***

Before you get further into DSB editing, here is some terminology:

- Archetypes and Instances  = The term "object" is somewhat ambiguous, so these
terms are used instead. An archetype (abbreviated arch) is a general category,
such as "Axe" or "Iron Keyhole." An instance (abbreviated inst) is one
specific thing that is found in the dungeon. "Screamer Slice" is an archetype,
one particular slice from a Screamer you just killed is an instance.

- Exvars = For reasons of speed and space, many instances don't have a full
representation in Lua. Instead, exvars (external variables) can be used to
store essential information about an instance that its archetype doesn't
have. For example, the power of a ful bomb, or the target of a pushbutton.

- Party position = Usually represented "ppos" in the code, this is a number
from 0 to 3 that refers to the slot occupied by a given character. This
is different than the id of the character him/herself.

- Subrenderer = A subrenderer is a special Lua function that renders the
large area in the lower-right corner of the inventory screen. In the default
distribution, subrenderers are used to create the chest and scroll, and
concievably about anything else could be built. Any efforts to reconstruct
the Amiga CSB magic map are quite welcome!

- System function = A system function is a Lua function that begins with the
prefix sys_, and is called by the game engine in order to do some task or
perform some calculation. They can be modified to change the game
behavior in subtle or dramatic ways.

- Boss = DSB Monsters are all individual entities. However, DSB attempts to
emulate DM's monster groups by appointing one monster on each tile the "boss."
The entire group moves and acts when the boss does, keeping the effect the
same.

***

For more information, visit the DM forum at:
http://www.dungeon-master.com/forum/

You'll find a DSB forum where you can read about functions that are
available and ask questions.

There are also quite a few useful functions provided in the default util.lua.
Please examine util.lua for more details.

***

Special Coordinates:
These are defined in global.lua.

PARTY = x is a Party position, y is in an inventory slot.
IN_OBJ = x is the inst id of the instance the targeted instance is inside, 
	y is a given slot (but can be 0)
CHARACTER = x is a Character id, y is an inventory slot.
MOUSE_HAND = x and y are ignored.
LIMBO = x and y are ignored.

In all cases, tile_position should be 0 for a spawn. It can usually be 0 for a fetch, 
but if you want the value to return a single-element table instead of a scalar 
(that is, behave like fetch on a dungeon cell), you can pass it "TABLE." 

***

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

DSB Copyright © 2007-2016 Amber Adams,
Dungeon Master Copyright © 1987 FTL.

This program utilizes several libraries:
Allegro Game Programming Library, © 1998-2016 Allegro contributors (alleg.sf.net)
Lua, © 1994-2016 Lua.org, PUC-Rio, (www.lua.org)
FMOD Sound System, © 1994-2016 Firelight Technologies (www.fmod.org)
Zlib, © 1995-2016 Jean-loup Gailly and Mark Adler
LibPNG, © 1995-2016 LibPNG contributors 
Mersenne Twister, © 2012 Mutsuo Saito, Makoto Matsumoto, Hiroshima University and The University of Tokyo.

In addition, it uses public domain LoadPNG code by Peter Wang.

Special thanks to:
Joramun, Remy, and kaypy from the DM.com forums for all of their Lua hacking,
debugging assistance, testing, and suggestions. 

Paul Stevens for CSBwin and helping to nail down some tricky bugs in DSB.

...and all of the other numerous people who have helped to test DSB!