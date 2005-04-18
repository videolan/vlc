== ACTIVEX Control for VLC ==

The VLC ActiveX Control has been primary designed to work with Internet Explorer.
however it may also work with Visual Basic and/or .NET
Please note, that this code does not rely upon Microsoft MFC/ATL code,
hence good compatibility is not guaranteed. 

== Compiling ==

The ActiveX Control should compile without any glitches as long as you have the latest
version of mingw gcc and headers. However, In order to script the ActiveX Control
on Internet Explorer, a type library is required.

This type library is usually generated from an IDL file using Microsoft MIDL compiler.
Therefore, for convenience I have checked in the output of the MIDL compiler in the
repository so that you will only need the MIDL compiler if you change axvlc.idl.
the generated files are as follow:

axvlc_idl.c
axvlc_idl.h
axvlc.tlb

To use the MIDL compiler on cygwin, you will need to set some environment variables
before configuring vlc. If you have a copy of 'Microsoft Visual C++ 6.0' installed,
the following settings are correct:

export PATH=$PATH:"/cygdrive/c/Program Files/Microsoft Visual Studio/COMMON/MSDev98/Bin":"/cygdrive/c/Program Files/Microsoft Visual Studio/VC98/Bin"
export INCLUDE='C:\Program Files\Microsoft Visual Studio\VC98\Include'
export MIDL="midl"

if you are cross-compiling on Linux, you may be able to use 'widl' which is part of
the WINE project (http://www.winehq.com), however I have not tested it.

== Debugging ==

the ActiveX control is compiled with verbose output by default. However you will
need to launch Internet Explorer from a Cygwin shell to see the output.

Debugging the ActiveX control DLL with GNU GDB can be difficult. Fortunately,
the ActiveX control can also be compiled as an executable rather than a DLL.
In ActiveX terms, this is called a local server.
The advantage of a local server is that it will never crash its client,
i.e Internet Explorer, if itself is crashing.
The build system does not currently allow to create an executable version of
the ActiveX control, you will need to manually define the BUILD_LOCALSERVER
pre-processor variable and modify the Makefile to exclude the '-shared' option
at the linking stage. Once this is done, you just need to launch axvlc.exe to
have a working Activex control. Please note, that executable version of the
ActiveX control will override any settings required for the DLL version, which
will no longer work until you (re)register it as shown in the following section

== Local Install ==

The VLC NSIS installer will install the ActiveX Control without requiring any
further manual intervention, but for people who like to live on the edge, here
are the steps you need to perform once you have built the ActiveX Control.

The ActiveX control DLL file may be copied anywhere on the target machine,
but before you can use the control, you will need to register it with Windows
by using the REGSVR32 command, as per following example:

REGSVR32 C:\WINDOWS\AXVLC.DLL

if the control needs to use external VLC plugins (i.e other than built-in ones),
make sure that the plugin path is set in the registry as per following example:

[HKEY_LOCAL_MACHINE\Software\VideoLAN\VLC]
InstallDir="C:\Program Files\VideoLAN\VLC"

The InstallDir must be the parent directory of the 'plugins' directory.

WARNING: Both control and plugins must come from the same build source tree.
Otherwise, at best, the control will not play any content, at worse
it may crash Internet Explorer while attempting to load incompatible plugins.

== Internet Install ==

The activex control may be installed from a remote through Internet Installer if
it is packaged up in a CAB file. The following link explains how to achieve this

http://msdn.microsoft.com/workshop/components/activex/packaging.asp

For convenience, I have provided a sample axvlc.INF file, which assumes that the VLC
NSIS Installer has been packaged up a CAB file called AXVLC.CAB.

The ActiveX Control DLL file can also be distributed by itself if it has been
compiled with built-in VLC plugins; check developer information for more
information on built-in plugins.

regards,
    Damien Fouilleul <Damien dot Fouilleul at laposte dot net>

