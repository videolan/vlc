This piece of software is based on the software and descriptions mentioned below -


(re)Written by:              Igor / Atmo (aka André Weber)  - WeberAndre@gmx.de
                             Matthiaz
                             MacGyver2k

if you need to contact one of us - come to www.vdr-portal.de

http://www.vdr-portal.de/board/thread.php?threadid=59294 -- Description and Development of the Windows Software
http://www.vdr-portal.de/board/thread.php?threadid=48574 -- Description and Development of the Hardware part

=====================================================
Info for Users on Vista or Windows 7 with active UAC!
=====================================================
for the first launch do it as Administrator with the following command line

AtmoWinA.exe /register

to create the required registry entries for the COM Objects, without
these registry entries - the DirectShow Filter and also the VLC Plugin
will not work.
If anything went fine you should see the message "COM Server registered Ok!".

See the file COPYING.txt for license information.


VideoLAN / VLC
---------------

In order to use this Programm with the VLC-Media-Player properly copy the "AtmoCtrlLib.dll" into the same Location where the vlc.exe is. E.G.: "D:\Programme\VLC".

NEWER Versions of VideoLAN 1.1.x and higher, don't need a own copy of the "AtmoCtrlLib.dll" in the vlc.exe folder,
if the complete filename of the AtmoWin*.exe is given. (Atmo Filter will try to load the DLL from there)

Then open VLC and navigate to "Extras" / "Settings" or Press "Ctrl" + "p".
Check that the radiobutton in the lower part of the dialogbox called "Show settings" -> "all" is checked.
Now open the Register "Video" and click on "Filter". There you have to select the checkbox "AtmoLight-Filter".
Afterwards you expand the Tree "Filter" in the Listbox on the left side. By selecting the entry "AtmoLight" you gain more settings.
The most important are right these one at the beginning:

VideoLAN 1.0.x
==============
"Use build in AtmoLight driver":
Using this Property means VLC will control the Hardware with its built in driver.
(no need for the extra AtmoWin software driver)

VideoLAN 1.1.x
==============
You have the following choices for "Devicetype" instead of the old settings
"AtmoWin Software" - is only available on Windows and works like the old way useing the external AtmoWin*.exe Software package, with the AtmoCtrlLib.dll.
"Classic AtmoLight" - is the same as the former "use build in AtmoLight driver"
"Quattro AtmoLight" - allows to use up to four serial connected class AtmoLight devices, as one logical device
"DMX" - allows to control a simple serial DMX devices for AtmoLight effects
"MoMoLight" - is a another kind of hardware supporting 2 oder 3 channels.
(see bottom of this file - for more infos about the devices.)

VideonLAN 1.0.x
===============
"Serial Device / Port":
The COM-Port you are using for the hardware. E.G.: COM6 (This setting must be done if you are using the Property
"use build in AtmoLight driver" )

VideonLAN 1.1.x
===============
"Serial Device / Port":
The COM-Port you are using for the hardware. E.G.: COM6 (This setting must be done if you are using a device
other than "AtmoWin Software")
in the case of the device "Quattro AtmoLight" you may specify up to four ports/devices separated by
, or ; for example if you are useing two physical devices write
COM6;COM7 or on linux /dev/ttyUSB1;/dev/ttyUSB2

VideoLAN 1.0.x
==============
"Filename of AtmoWinA.exe":
The Path and the Name of the executable. E.G.:
'D:\atmoWin_0.45\AtmoWinA.exe' (Using this setting expects that you have copied "AtmoCtrlLib.dll" to the Path where VLC.exe is like described before)

VideoLAN 1.1.x
==============
"Filename of AtmoWinA.exe":
The Path and the Name of the executable. E.G.:
'D:\atmoWin_0.45\AtmoWinA.exe'
in the same path where the .exe resides the "AtmoCtrlLib.dll" should be there. or you may need to place
a copy of this ".dll" into the vlc.exe folder - like in VideoLAN 1.0.x

Afterwards you should apply those settings by pressing the button "Save".




VideoLAN 1.1.x - new options and settings
=========================================
Zone Layout for the build-in Atmo
=================================
This is the most important change I think - because the number of analyzed zones isn't
longer fixed to 4 or 5 - it allows to define the physical layout of your lights arround
the screen.
"Number of zones on top"     -- how many light sources are place on top of the screen
                               (number of columns in which the screen should be tiled)

"Number of zones on bottom"  -- how many light sources are place on bottom of the screen
                               (number of columns in which the screen should be tiled)

"Zones on left / right side" -- how many light sources are place on right/left of the screen
                               (number of rows in which the screen should be tiled)
                               its assumed that the number on right and left is equal.

"[] Calculate a average zone" -- define a color pair depending on the complete screen content

This settings tiles the screen into a number of zones - which get analyzed by atmoLight to
determine a most used color for these areas - each zone has a number starting from zero.
The zones are usualy numbered in clock-wise order starting at the top/left of the screen.

Example 1: classic Atmo with 4 channels
---------------------------------------
"Number of zones on top" = 1
"Number of zones on bottom" = 1
"Zones on left / right side" = 1
"[] Calculate a average zone" = false/0 not checked.

will produce this zone layout
     -----------
     | Zone 0  |
---------------------
|   |           |   |
| Z |           | Z |
| o |           | o |
| n |           | n |
| e |           | e |
|   |           |   |
| 3 |           | 1 |
---------------- ----
     | Zone 2  |
     -----------


Example 2: classic Atmo with 4 channels
---------------------------------------
"Number of zones on top" = 2
"Number of zones on bottom" = 0
"Zones on left / right side" = 1
"[] Calculate a average zone" = false/0 not checked.

     ----------- -----------
     | Zone 0  | | Zone 1  |
---------------------------------
|   |                       |   |
| Z |                       | Z |
| o |                       | o |
| n |                       | n |
| e |                       | e |
|   |                       |   |
| 3 |                       | 2 |
-----                       -----

Example 3: classic Atmo with 4 channels
---------------------------------------
"Number of zones on top" = 1
"Number of zones on bottom" = 0
"Zones on left / right side" = 1
"[X] Calculate a average zone" = true/1 checked.

     -----------
     | Zone 0  |
---------------------
|   | --------  |   |
| Z | |  Z    | | Z |
| o | |  o    | | o |
| n | |  n    | | n |
| e | |  e    | | e |
|   | |  3    | |   |
| 2 | --------  | 1 |
-----           -----
Zone 3 - usualy calcuates the most used color of the full screen / picture / frame
not only at the border of the picture.

"The average zone" is allways the last in the sequence of numbers.



the weightning gradients for these zones are auto calculated
from 100% .. 0% starting from the edge.

thats also the cause why the parameters  Channel assignment changed,
the classic comboboxes are still there for devices with 4 channels ot less,
but for newer devices the "Channel / Zone assignment" should be used which
is defined by a , or ; separated list of "AtmoLight channel numbers" if you
want to hide a calcuated zone from output - you can assign channel -1 to
do this.
for classic AtmoLight with "Example 1" you may write this:
-1;3;2;1;0
AtmoLight Channel 0: gets no zone assigned (-1)
AtmoLight Channel 1: gets zone 3 (left)
AtmoLight Channel 2: gets zone 2 (bottom)
AtmoLight Channel 3: gets zone 1 (right)
AtmoLight Channel 4: gets zone 0 (top)


Also the settings for Gradient images change for the new devices, its no longer
sufficient to speficy only 5 image name - because the number of zones is no longer
fixed to five.
So its preferred to set a path ("Gradient Bitmap searchpath"), where files
like "zone_0.bmp" "zone_1.bmp" etc. exists. (with the same rules as defined for
the old zone bitmaps.)
--> I think in most cases its no longer required to use this option,
to change the zone layout - for most cases its sufficient to change
"Zone Layout for the build-in Atmo" to get the same effect?

Live Set of parameters for Buildin AtmoLight
============================================
durring playback with the buildin driver you can now change some settings
of the filter without stopping / starting your movie - just open the
"extras" --> "Effects and Filters"  --> [Video effects] --> [AtmoLight]
- move the sliders or change the calcuation mode, or enable/disable
the debug grid - just in time.


new Debugging Option
====================
[] Mark analyzed pixels - puts a grid of the used pixels on each frame
to see from which locations/positions the colors are extracted.

DMX Options
-----------
like the group says only for the DMX device
"Count of AtmoLight Channels" - defines how many RGB Channels should be simulated
with this DMX device (each RGB channel needs three DMX channels!)

"DMX address for each channel" - defines the DMX Startadress for each AtmoLight
channel as "," or ";" separated list. (starting with 0 up to 252) it is assumed
that the f.e. of the DMX-AtmoLight channel starts at DMX-Channel 5 - that
DMX-Channel 5: is red
DMX-Channel 6: is green
DMX-Channel 7: is blue!

MoMoLight options
-----------------
"Count of channels" - defines the devicetype and serial protocol
3: - means 3 channels hardware
4: - means 4 channels hardware
(its required to set the correct number of channels to get this device working,
 because the serial protocol isn't the same!)

Fnordlicht options
------------------
"Count of fnordlicht's" - defines the number of devices connected to the bus



VideoLan Options and Devices - the buildin version of AtmoLight supports a subset of the
devices that AtmoWin supports.

- AtmoWin Software - means do not use the VLC buildin video processing - just forward
  the basic preprocessed data to the launched AtmoWinA.exe controlling your hardware.

- Classic AtmoLight - means the classic hardware from www.vdr-portal.de - with up to 5 channels.

- Quattro AtmoLight - is nothing else as that you have connected up to 4 "classic AtmoLight" devices
  to your computer creating a up 16 channel Atmo Light - each devices needs its own serial port.
  you have to write the ports separated by , or ; to [Serial Port/device] f.e. COM3,COM4,COM5 or on
  Linux /dev/ttyUSB01,/dev/ttyUSB02,/dev/ttyUSB03

- DMX - stands for a simple DMX controller which can control up to 255 DMX devices (lights)
  - f.e. you may have a look here:
     * http://www.dzionsko.de/elektronic/index.htm
     * http://www.ulrichradig.de/ (search for dmx on his page)

- MoMoLight - is a serial device, with 3 or 4 channels - doing nearly the same like
  Classic AtmoLight - just another protocol to control the hardware.
   http://lx.divxstation.com/article.asp?aId=151
   http://www.the-boss.dk/pages/momolight.htm

- Fnordlicht - is a serial device bus, where up 254 lights could be connected
  for more information about the device look here:
   http://wiki.lochraster.org/wiki/Fnordlicht
   http://github.com/fd0/fnordlicht/raw/master/doc/PROTOCOL



Original Readme - of the Linux Version - from where some code was used
to do the color calculations ...

######################################################################
Original Readme and Authors of the Linux VDR Plugin!
######################################################################

Written by:                  Eike Edener   <vdr@edener.de>
Project's homepage:          www.edener.de
Latest version available at: www.edener.de
See the file COPYING for license information.
----------------------------------------------------------------------

for detailed information visit the VDR-Wiki:
http://www.vdr-wiki.de/wiki/index.php/Atmo-plugin

Development:
http://www.vdr-portal.de/board/thread.php?threadid=48574

Known bugs:
n/a

----------------------------------------------------------------------
