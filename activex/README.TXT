== ACTIVEX Control for VLC ==

The VLC ActiveX Control has been primary designed to work with Internet Explorer.
however it may also work with Visual Basic and/or .NET
Please note, that this code does not rely upon MFC/ATL, hence good compatibility
is not guaranteed. 

== Compiling ==

The Active Control should compile without any glitches as long as you have the latest
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

== Local Install ==

The ActiveX control is built as a DLL file, which is the only file that needs be
distributed and it may be installed anywhere on the target machine as long as 
its path is specified within your PATH environment variable. Typically, you would
install it into your WINDOWS directory.

if the control needs other VLC plugins, you will need to have VLC installed
on your system, otherwise make sure that the plugin path is set in the registry
as indicated below

[HKEY_LOCAL_MACHINE\Software\VideoLAN\VLC]
InstallDir="path\\dir\\"

WARNING: The plugins version must also match the ActiveX control version,
otherwise it may crash while attempting to load incompatible plugins

== Internet Install ==

The activex control may be installed from a remote through Internet Installer if
it is packaged up in a CAB file. The following link explains how to achieve this

http://msdn.microsoft.com/workshop/components/activex/packaging.asp

For convenience, I have provide a sample axvlc.INF file, which installs the ActiveX Control
into the WINDOWS directory, feel free to adapt it to your need.

WARNING: For remote install, the ActiveX Control should be built with all required VLC
plugins built-in.

regards,
    Damien Fouilleul <Damien dot Fouilleul at laposte dot net>

