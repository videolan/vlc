== ACTIVEX Control for VLC ==

The VLC ActiveX Control has been primary designed to work with Internet
Explorer. However it may also work with Visual Basic and/or .NET. Please
note, that this code does not rely upon Microsoft MFC/ATL code, hence
good compatibility is not guaranteed.

I. Compiling

The ActiveX Control should compile without any glitches as long as you
have the latest version of mingw gcc and headers.

In order to script the ActiveX Control on Internet Explorer, a type
library is required. This type library is usually generated from an IDL
file using Microsoft MIDL compiler. Therefore, for convenience I have
checked in the output of the MIDL compiler in the repository so that you
will only need the MIDL compiler if you change axvlc.idl. the generated
files are as follow:

axvlc_idl.c
axvlc_idl.h
axvlc.tlb

To use the MIDL compiler on cygwin, you will need to set some
environment variables before configuring vlc. If you have a copy of
'Microsoft Visual C++ 6.0' installed, the following settings are
correct:

export PATH=$PATH:"/cygdrive/c/Program Files/Microsoft Visual Studio/COMMON/MSDev98/Bin":"/cygdrive/c/Program Files/Microsoft Visual Studio/VC98/Bin"
export INCLUDE='C:\Program Files\Microsoft Visual Studio\VC98\Include'
export MIDL="midl"

If you are cross-compiling on Linux, you can use 'widl' which is part of
the WINE project (http://www.winehq.com). At leat wine-dev-0.9.57 works,
the comand line to compile IDL should looks like the following :

widl -I/usr/include/wine/windows/ \
	-h -H axvlc_idl.h -t -T axvlc.tlb -u -U axvlc_idl.c axvlc.idl

NOTE: widl breaks compatibility with Visual Basic. If that is important
to you then you must use midl.

II. Debugging

The ActiveX control is compiled with verbose output by default, but you
will need to launch Internet Explorer from a Cygwin shell to see the
output. Alternatively, the plugin will also use the VLC preferences, so
if you enable the file logging interface through the player and save the
preferences, the control will automatically log its verbose output into
the designated file.

Debugging the ActiveX control DLL with GNU GDB can be difficult.
Fortunately the ActiveX control can also be compiled as an executable
rather than a DLL. In ActiveX terms, this is called a local server. The
advantage of a local server is that it will never crash its client,
i.e. Internet Explorer, even if the local server crashes. The build
system does not currently allow to create an executable version of the
ActiveX control, you will need to manually define the BUILD_LOCALSERVER
pre-processor variable and modify the Makefile to exclude the '-shared'
option at the linking stage. Once this is done, launch axvlc.exe to have
a working Activex control. Please note, that executable version of the
ActiveX control will override any settings required for the DLL version,
which will no longer work until you (re)register it as shown in the
following section

III. Local Install

The VLC NSIS installer will install the ActiveX Control without
requiring any further manual intervention, but for people who like to
live on the edge, here are the steps you need to perform once you have
built the ActiveX Control.

The ActiveX control DLL file may be copied anywhere on the target
machine, but before you can use the control, you will need to register
it with Windows by using the REGSVR32 command, as per following example:

REGSVR32 C:\WINDOWS\AXVLC.DLL

If the control needs to use external VLC plugins (i.e other than the
built-in ones), make sure that the plugin path is set in the registry as
per following example:

[HKEY_LOCAL_MACHINE\Software\VideoLAN\VLC]
InstallDir="C:\Program Files\VideoLAN\VLC"

The InstallDir must be the parent directory of the 'plugins' directory.

WARNING: Both control and plugins must come from the same source build
tree. Otherwise, at best, the control will not play any content,
at worse it may crash Internet Explorer while attempting to load
incompatible plugins.

IV. Internet Install

The activex control may be installed from a remote through Internet
Installer if it is packaged up in a CAB file. The following link
explains how to achieve this

http://msdn.microsoft.com/workshop/components/activex/packaging.asp

For convenience, I have provided a sample axvlc.INF file, which assumes
that the VLC NSIS Installer has been packaged up a CAB file called
AXVLC.CAB.

The ActiveX Control DLL file can also be distributed by itself if it has
been compiled with built-in VLC plugins; check developer information for
more information on built-in plugins.

V. Controlling the plugin

1) Properties

The following public properties can be used to control the plugin
from HTML, the property panel of Visual Basic and most ActiveX aware
applications.

+==========+=========+===================================+===============+
| Name:    | Type:   |   Description:                    | Alias:        |
+==========+=========+===================================+===============+
| autoplay | boolean | play when control is activated    | autostart     |
+----------+---------+-----------------------------------+---------------+
| autoloop | boolean | loop the playlist                 | loop          |
+----------+---------+-----------------------------------+---------------+
| mrl      | string  | initial MRL in playlist           | src, filename |
+----------+---------+-----------------------------------+---------------+
| mute     | boolean | mute audio volume                 |               |
+----------+---------+-----------------------------------+---------------+
| visible  | boolean | show/hide control viewport        | showdisplay   |
+----------+---------+-----------------------------------+---------------+
| volume   | integer | set/get audio volume              |               |
+----------+---------+-----------------------------------+---------------+
| toolbar  | boolean | set/get visibility of the toolbar |               |
+----------+---------+-----------------------------------+---------------+

The alias column shows an alternative <PARAM name> for the property in
internet explorer, which is useful to maintain compatibility with HTML
pages already leveraging Windows Media Player

2) Programming APIs

The MRL, Autoplay and Autoloop properties are only used to configure the
initial state of the ActiveX control,i.e before its activation; they are
ignored afterward. Therefore, if some runtime control is required, the
following APIs should be used within your programming environment:

Variables:

+==========+=========+=========+=======================================+
| Name:    | Type:   | Access: | Description:                          |
+==========+=========+=========+=======================================+
| Playing  | boolean |   RO    | Returns whether some MRL is playing   |
+----------+---------+---------+---------------------------------------+
| Time     | integer |   RW    | Time elapsed in seconds playing       |
|          |         |         | current MRL                           |
|          |         |         | NOTE: live feeds returns 0            |
+----------+---------+---------+---------------------------------------+
| Position | real    |   RW    | Playback position within current MRL  |
|          |         |         | in a scale from 0.0 to 1.0            |
|          |         |         | NOTE: live feeds returns 0.0          |
+----------+---------+---------+---------------------------------------+
| Length   | integer |   RO    | Total length in seconds of current MRL|
|          |         |         | NOTE: live feeds returns 0            |
+----------+---------+---------+---------------------------------------+
| Volume   | integer |   RW    | Current volume from 0 to 100          |
+----------+---------+---------+---------------------------------------+
| Visible  | boolean |   RW    | Indicates whether control is visible  |
+----------+---------+---------+---------------------------------------+

Methods:

  *** current interface (0.8.6+) ***
UUID : 9BE31822-FDAD-461B-AD51-BE1D1C159921
defined in axvlc.idl as "coclass VLCPlugin2", "interface IVLCControl2"

This interface organizes an API with several objects (like .audio.mute).
It is currently documented on videolan wiki (the url may change) at
http://wiki.videolan.org/Documentation:Play_HowTo/Advanced_Use_of_VLC


  ***  old interface (deprecated)  ***
UUID : E23FE9C6-778E-49D4-B537-38FCDE4887D8
defined in axvlc.idl as "coclass VLCPlugin", "interface IVLCControl"

play()
    Play current item the playlist

pause()
    Pause current item in the playlist

stop()
    Stop playing current item in playlist

shuttle(Seconds as integer)
    Advance/backtrack playback by specified amount (which is negative for
    backtracking). This is also called relative seeking.
    This method does not work for live streams.

fullscreen()
    Switch between normal and full screen video

playFaster()
    Increase play back speed by 2X, 4X, 8X

playSlower()
    Decrease play back speed by 2X, 4X, 8X

toggleMute()
    mute/unmute sound output

addTarget(MRL As String, Options as array of strings,
          Mode as enumeration, Position as integer)
    Add an MRL into the default playlist, you can also specify a list
    of playlist options to attach to this MRL or Null for no options.
    Mode indicates the action taken by the playlist on MRL and is one
    the following:

        VLCPlayListInsert       =  1 (Insert MRL into playlist at Position)
        VLCPlayListInsertAndGo  =  9 (Insert MRL into playlist at Position and play it immediately)
        VLCPlayListReplace      =  2 (Replace MRL in playlist at Position)
        VLCPlayListReplaceAndGo = 10 (Replace MRL in playlist at Position and play it immediately)
        VLCPlayListAppend       =  4 (Append MRL in playlist after Position)
        VLCPlayListAppendAndGo  = 12 (Append MRL in playlist after Position and play it immediately)
        VLCPlayListCheckInsert  = 16 (Verify if MRL is in playlist) 

    Position can take the value of -666 as wildcard for the last element
    in the playlist.


setVariable(Name as string, Value as object);
    Set a value into a VLC variables

getVariable(Name as string) as object
    Retrieve the value of a VLC variable.

Regards,
    Damien Fouilleul <Damien dot Fouilleul at laposte dot net>

