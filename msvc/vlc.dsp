# Microsoft Developer Studio Project File - Name="vlc" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=vlc - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "vlc.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "vlc.mak" CFG="vlc - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "vlc - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "vlc - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "vlc - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD -I..\include /c
# ADD BASE RSC /l 0x414 /d "NDEBUG"
# ADD RSC /l 0x414 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib netapi32.lib winmm.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /GZ -I..\include /c
# ADD BASE RSC /l 0x414 /d "_DEBUG"
# ADD RSC /l 0x809 /i "../" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib netapi32.lib winmm.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "vlc - Win32 Release"
# Name "vlc - Win32 Debug"

# Begin Group "Source Files"
# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "vlc"
# Begin Source File
SOURCE="..\src\vlc.c"
# End Source File
# Begin Source File
SOURCE="..\extras\GNUgetopt\getopt.c"
# End Source File
# Begin Source File
SOURCE="..\extras\GNUgetopt\getopt.h"
# End Source File
# Begin Source File
SOURCE="..\extras\GNUgetopt\getopt1.c"
# End Source File
# End Group
# Begin Group "libvlc"
# Begin Source File
SOURCE="..\src\libvlc.c"
# ADD CPP /D "__VLC__" /D PLUGIN_PATH=\"plugins\" /D DATA_PATH=\"share\"
# End Source File
# Begin Source File
SOURCE="..\src\libvlc.h"
# ADD CPP /D "__VLC__" /D PLUGIN_PATH=\"plugins\" /D DATA_PATH=\"share\"
# End Source File
# Begin Group "audio_output"
# Begin Source File
SOURCE="..\src\audio_output\common.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\audio_output"
# PROP Output_Dir "Release\src\audio_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\audio_output"
# PROP Output_Dir "Debug\src\audio_output"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\audio_output\dec.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\audio_output"
# PROP Output_Dir "Release\src\audio_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\audio_output"
# PROP Output_Dir "Debug\src\audio_output"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\audio_output\filters.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\audio_output"
# PROP Output_Dir "Release\src\audio_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\audio_output"
# PROP Output_Dir "Debug\src\audio_output"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\audio_output\input.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\audio_output"
# PROP Output_Dir "Release\src\audio_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\audio_output"
# PROP Output_Dir "Debug\src\audio_output"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\audio_output\mixer.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\audio_output"
# PROP Output_Dir "Release\src\audio_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\audio_output"
# PROP Output_Dir "Debug\src\audio_output"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\audio_output\output.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\audio_output"
# PROP Output_Dir "Release\src\audio_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\audio_output"
# PROP Output_Dir "Debug\src\audio_output"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\audio_output\intf.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\audio_output"
# PROP Output_Dir "Release\src\audio_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\audio_output"
# PROP Output_Dir "Debug\src\audio_output"
!ENDIF
# End Source File
# End Group
# Begin Group "input"
# Begin Source File
SOURCE="..\src\input\input.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\input"
# PROP Output_Dir "Release\src\input"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\input"
# PROP Output_Dir "Debug\src\input"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\input\input_ext-plugins.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\input"
# PROP Output_Dir "Release\src\input"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\input"
# PROP Output_Dir "Debug\src\input"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\input\input_ext-dec.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\input"
# PROP Output_Dir "Release\src\input"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\input"
# PROP Output_Dir "Debug\src\input"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\input\input_ext-intf.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\input"
# PROP Output_Dir "Release\src\input"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\input"
# PROP Output_Dir "Debug\src\input"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\input\input_dec.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\input"
# PROP Output_Dir "Release\src\input"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\input"
# PROP Output_Dir "Debug\src\input"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\input\input_programs.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\input"
# PROP Output_Dir "Release\src\input"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\input"
# PROP Output_Dir "Debug\src\input"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\input\input_clock.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\input"
# PROP Output_Dir "Release\src\input"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\input"
# PROP Output_Dir "Debug\src\input"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\input\input_info.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\input"
# PROP Output_Dir "Release\src\input"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\input"
# PROP Output_Dir "Debug\src\input"
!ENDIF
# End Source File
# End Group
# Begin Group "interface"
# Begin Source File
SOURCE="..\src\interface\interface.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\interface"
# PROP Output_Dir "Release\src\interface"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\interface"
# PROP Output_Dir "Debug\src\interface"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\interface\intf_eject.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\interface"
# PROP Output_Dir "Release\src\interface"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\interface"
# PROP Output_Dir "Debug\src\interface"
!ENDIF
# End Source File
# End Group
# Begin Group "misc"
# Begin Source File
SOURCE="..\src\misc\mtime.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\modules.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\threads.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\cpu.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\configuration.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\netutils.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\iso_lang.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\iso-639.def"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\messages.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\objects.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\variables.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\error.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\extras.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\misc\win32_specific.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\misc"
# PROP Output_Dir "Release\src\misc"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\misc"
# PROP Output_Dir "Debug\src\misc"
!ENDIF
# End Source File
# End Group
# Begin Group "playlist"
# Begin Source File
SOURCE="..\src\playlist\playlist.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\playlist"
# PROP Output_Dir "Release\src\playlist"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\playlist"
# PROP Output_Dir "Debug\src\playlist"
!ENDIF
# End Source File
# End Group
# Begin Group "stream_output"
# Begin Source File
SOURCE="..\src\stream_output\stream_output.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\stream_output"
# PROP Output_Dir "Release\src\stream_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\stream_output"
# PROP Output_Dir "Debug\src\stream_output"
!ENDIF
# End Source File
# End Group
# Begin Group "video_output"
# Begin Source File
SOURCE="..\src\video_output\video_output.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\video_output"
# PROP Output_Dir "Release\src\video_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\video_output"
# PROP Output_Dir "Debug\src\video_output"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\video_output\vout_pictures.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\video_output"
# PROP Output_Dir "Release\src\video_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\video_output"
# PROP Output_Dir "Debug\src\video_output"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\video_output\vout_pictures.h"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\video_output"
# PROP Output_Dir "Release\src\video_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\video_output"
# PROP Output_Dir "Debug\src\video_output"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\video_output\video_text.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\video_output"
# PROP Output_Dir "Release\src\video_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\video_output"
# PROP Output_Dir "Debug\src\video_output"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\video_output\video_text.h"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\video_output"
# PROP Output_Dir "Release\src\video_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\video_output"
# PROP Output_Dir "Debug\src\video_output"
!ENDIF
# End Source File
# Begin Source File
SOURCE="..\src\video_output\vout_subpictures.c"
!IF "$(CFG)" == "vlc - Win32 Release"
# PROP Intermediate_Dir "Release\src\video_output"
# PROP Output_Dir "Release\src\video_output"
!ELSEIF "$(CFG)" == "vlc - Win32 Debug"
# PROP Intermediate_Dir "Debug\src\video_output"
# PROP Output_Dir "Debug\src\video_output"
!ENDIF
# End Source File
# End Group
# End Group
# End Group
# Begin Group "Header Files"
# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File
SOURCE="..\include\aout_internal.h"
# End Source File
# Begin Source File
SOURCE="..\include\audio_output.h"
# End Source File
# Begin Source File
SOURCE="..\include\beos_specific.h"
# End Source File
# Begin Source File
SOURCE="..\include\configuration.h"
# End Source File
# Begin Source File
SOURCE="..\include\darwin_specific.h"
# End Source File
# Begin Source File
SOURCE="..\include\codecs.h"
# End Source File
# Begin Source File
SOURCE="..\include\error.h"
# End Source File
# Begin Source File
SOURCE="..\include\input_ext-dec.h"
# End Source File
# Begin Source File
SOURCE="..\include\input_ext-intf.h"
# End Source File
# Begin Source File
SOURCE="..\include\input_ext-plugins.h"
# End Source File
# Begin Source File
SOURCE="..\include\interface.h"
# End Source File
# Begin Source File
SOURCE="..\include\intf_eject.h"
# End Source File
# Begin Source File
SOURCE="..\include\iso_lang.h"
# End Source File
# Begin Source File
SOURCE="..\include\main.h"
# End Source File
# Begin Source File
SOURCE="..\include\mmx.h"
# End Source File
# Begin Source File
SOURCE="..\include\modules.h"
# End Source File
# Begin Source File
SOURCE="..\include\modules_inner.h"
# End Source File
# Begin Source File
SOURCE="..\include\mtime.h"
# End Source File
# Begin Source File
SOURCE="..\include\netutils.h"
# End Source File
# Begin Source File
SOURCE="..\include\network.h"
# End Source File
# Begin Source File
SOURCE="..\include\os_specific.h"
# End Source File
# Begin Source File
SOURCE="..\include\stream_control.h"
# End Source File
# Begin Source File
SOURCE="..\include\stream_output.h"
# End Source File
# Begin Source File
SOURCE="..\include\variables.h"
# End Source File
# Begin Source File
SOURCE="..\include\video.h"
# End Source File
# Begin Source File
SOURCE="..\include\video_output.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc_common.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc_config.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc_cpu.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc_messages.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc_objects.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc_playlist.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc_threads.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc_threads_funcs.h"
# End Source File
# Begin Source File
SOURCE="..\include\win32_specific.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc_symbols.h"
# End Source File
# Begin Group "vlc"
# Begin Source File
SOURCE="..\include\vlc\vlc.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc\aout.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc\vout.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc\sout.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc\decoder.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc\input.h"
# End Source File
# Begin Source File
SOURCE="..\include\vlc\intf.h"
# End Source File
# End Group
# End Group

# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\share\vlc_win32_rc.rc

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# ADD BASE RSC /l 0x40c /i "\vlc-win32\share" /i "\vlc-win\share"
# SUBTRACT BASE RSC /i "../"
# ADD RSC /l 0x40c /i "\vlc-win32\share" /i "\vlc-win\share" /i "../../"
# SUBTRACT RSC /i "../"

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
