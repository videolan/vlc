== ACTIVEX Control for VLC ==

The VLC ActiveX Control has been primary designed to work with Internet Explorer.
however it may also work with Visual Basic and/or .NET
Please note, that this code does not rely upon MFC/ATL, hence good compatibility
is not guaranteed

== Compiling ==

In order to script the ActiveX Control on Internet Explorer, a type library is required.
This type library is usually generated from an IDL file using Microsoft MIDL compiler.
However, for convenience I have checked in the output of the MIDL compiler in the
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

== Installing ==

The ActiveX control is built as axvlc.dll, which is the only file that needs be
distributed and it may be installed anywhere on the target machine as long as 
its path is correct within the registry.
the necessary registry settings are stored in axvlc.reg

in order to use the plugin, you will need to VLC properly installed on your system
otherwise make sure that the plugin path is set in the registry as indicated below

[HKEY_LOCAL_MACHINE\Software\VideoLAN\VLC]
InstallDir="path\\dir\\"

regards,
    Damien Fouilleul <Damien dot Fouilleul at laposte dot net>
