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
# ADD CPP /nologo /MT /W3 /GX /O2 /I "." /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /D "__VLC__" /YX /FD /I../../include /c
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
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /I "." /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D "__VLC__" /FR /YX /FD /GZ /I../../include /c
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
# Begin Group "extras"

# PROP Default_Filter ""
# Begin Group "GNUgetopt"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\extras\GNUgetopt\getopt.c
# End Source File
# Begin Source File

SOURCE=..\..\extras\GNUgetopt\getopt.h
# End Source File
# Begin Source File

SOURCE=..\..\extras\GNUgetopt\getopt1.c
# End Source File
# End Group
# Begin Group "dirent"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\dirent.c
# End Source File
# End Group
# Begin Group "libdvdcss"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\libdvdcss\src\common.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libdvdcss\src\config.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libdvdcss\src\css.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libdvdcss\src\css.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libdvdcss\src\csstables.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libdvdcss\src\dvdcss\dvdcss.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libdvdcss\src\ioctl.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libdvdcss\src\ioctl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\libdvdcss\src\libdvdcss.c
# End Source File
# Begin Source File

SOURCE=..\..\..\libdvdcss\src\libdvdcss.h
# End Source File
# End Group
# End Group
# Begin Group "plugins"

# PROP Default_Filter ""
# Begin Group "a52"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\a52\a52.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=a52 /D "USE_A52DEC_TREE" /I../a52dec

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=a52 /D "USE_A52DEC_TREE" /I../a52dec

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\a52\a52.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "ac3_adec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_adec.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=ac3_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_adec.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_bit_allocate.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=ac3_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_decoder.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=ac3_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_decoder.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_exponent.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=ac3_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_exponent.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_imdct.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=ac3_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_internal.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_mantissa.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=ac3_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_mantissa.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_parse.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=ac3_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\ac3_adec\ac3_rematrix.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=ac3_adec
# End Source File
# End Group
# Begin Group "access"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\access\file.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=file
# End Source File
# Begin Source File

SOURCE=..\..\plugins\access\http.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=http
# End Source File
# Begin Source File

SOURCE=..\..\plugins\access\udp.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=udp
# End Source File
# End Group
# Begin Group "chroma"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\chroma\i420_rgb.c
# ADD CPP /D "__BUILTIN__" /D "MODULE_NAME_IS_chroma_i420_rgb" /D MODULE_NAME=chroma_i420_rgb
# End Source File
# Begin Source File

SOURCE=..\..\plugins\chroma\i420_rgb.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\chroma\i420_rgb16.c
# ADD CPP /D "__BUILTIN__" /D "MODULE_NAME_IS_chroma_i420_rgb" /D MODULE_NAME=chroma_i420_rgb
# End Source File
# Begin Source File

SOURCE=..\..\plugins\chroma\i420_rgb8.c
# ADD CPP /D "__BUILTIN__" /D "MODULE_NAME_IS_chroma_i420_rgb" /D MODULE_NAME=chroma_i420_rgb
# End Source File
# Begin Source File

SOURCE=..\..\plugins\chroma\i420_rgb_c.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\chroma\i420_yuy2.c
# ADD CPP /D "__BUILTIN__" /D "MODULE_NAME_IS_chroma_i420_yuy2" /D MODULE_NAME=chroma_i420_yuy2
# End Source File
# Begin Source File

SOURCE=..\..\plugins\chroma\i420_yuy2.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\chroma\i422_yuy2.c
# ADD CPP /D "__BUILTIN__" /D "MODULE_NAME_IS_chroma_i422_yuy2" /D MODULE_NAME=chroma_i422_yuy2
# End Source File
# Begin Source File

SOURCE=..\..\plugins\chroma\i422_yuy2.h
# End Source File
# End Group
# Begin Group "directx"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\directx\aout_directx.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=directx
# End Source File
# Begin Source File

SOURCE=..\..\plugins\directx\directx.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=directx
# End Source File
# Begin Source File

SOURCE=..\..\plugins\directx\vout_directx.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=directx
# End Source File
# Begin Source File

SOURCE=..\..\plugins\directx\vout_directx.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\directx\vout_events.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=directx
# End Source File
# End Group
# Begin Group "downmix"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\downmix\ac3_downmix_c.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=downmix
# End Source File
# Begin Source File

SOURCE=..\..\plugins\downmix\ac3_downmix_common.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\downmix\downmix.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=downmix
# End Source File
# End Group
# Begin Group "dummy"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\dummy\aout_dummy.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=dummy
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dummy\dummy.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=dummy
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dummy\input_dummy.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=dummy
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dummy\intf_dummy.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=dummy
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dummy\null.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=null
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dummy\vout_dummy.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=dummy
# End Source File
# End Group
# Begin Group "dvd"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd.c
# ADD CPP /I "../../include" /I "../../../libdvdcss/src" /D "__BUILTIN__" /D MODULE_NAME=dvd
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_access.c
# ADD CPP /I "../../include" /I "../../../libdvdcss/src" /D "__BUILTIN__" /D MODULE_NAME=dvd
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_demux.c
# ADD CPP /I "../../include" /I "../../../libdvdcss/src" /D "__BUILTIN__" /D MODULE_NAME=dvd
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_es.c
# ADD CPP /I "../../include" /I "../../../libdvdcss/src" /D "__BUILTIN__" /D MODULE_NAME=dvd
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_es.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_ifo.c
# ADD CPP /I "../../include" /I "../../../libdvdcss/src" /D "__BUILTIN__" /D MODULE_NAME=dvd
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_ifo.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_seek.c
# ADD CPP /I "../../include" /I "../../../libdvdcss/src" /D "__BUILTIN__" /D MODULE_NAME=dvd
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_seek.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_summary.c
# ADD CPP /I "../../include" /I "../../../libdvdcss/src" /D "__BUILTIN__" /D MODULE_NAME=dvd
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_summary.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_udf.c
# ADD CPP /I "../../include" /I "../../../libdvdcss/src" /D "__BUILTIN__" /D MODULE_NAME=dvd
# End Source File
# Begin Source File

SOURCE=..\..\plugins\dvd\dvd_udf.h
# End Source File
# End Group
# Begin Group "filter"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\filter\deinterlace.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=deinterlace

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=deinterlace

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\filter\distort.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=distort

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=distort

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\filter\filter_common.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\filter\invert.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=insert

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=insert

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\filter\transform.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=transform

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=transform

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\filter\wall.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=wall

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=wall

!ENDIF 

# End Source File
# End Group
# Begin Group "fx"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\fx\scope.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=scope
# End Source File
# End Group
# Begin Group "gtk"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_callbacks.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_callbacks.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_common.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_control.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_control.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_display.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_display.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_interface.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_interface.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_menu.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_menu.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_modules.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_modules.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_open.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_open.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_playlist.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_playlist.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_preferences.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_preferences.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_support.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=gtk

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\gtk\gtk_support.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# End Group
# Begin Group "idct"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\idct\block_c.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\idct\idct.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=idct
# End Source File
# Begin Source File

SOURCE=..\..\plugins\idct\idct.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\idct\idct_decl.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\idct\idct_sparse.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\idct\idctclassic.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=idctclassic
# End Source File
# End Group
# Begin Group "imdct"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\imdct\ac3_imdct_c.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=imdct
# End Source File
# Begin Source File

SOURCE=..\..\plugins\imdct\ac3_imdct_common.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=imdct
# End Source File
# Begin Source File

SOURCE=..\..\plugins\imdct\ac3_imdct_common.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\imdct\ac3_retables.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\imdct\ac3_srfft.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\imdct\ac3_srfft_c.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=imdct
# End Source File
# Begin Source File

SOURCE=..\..\plugins\imdct\imdct.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=imdct
# End Source File
# End Group
# Begin Group "lpcm_adec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\lpcm_adec\lpcm_adec.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=lpcm_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\lpcm_adec\lpcm_adec.h
# End Source File
# End Group
# Begin Group "memcpy"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\memcpy\fastmemcpy.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\memcpy\memcpy.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=memcpy

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=memcpy /D "MODULE_NAME_IS_memcpy"

!ENDIF 

# End Source File
# End Group
# Begin Group "motion"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\motion\motion.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=motion
# End Source File
# End Group
# Begin Group "mpeg_adec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\mpeg_adec\adec_layer1.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_adec\adec_layer1.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_adec\adec_layer2.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_adec\adec_layer2.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_adec\adec_math.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_adec\adec_math.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_adec\mpeg_adec.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_adec\mpeg_adec.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_adec\mpeg_adec_generic.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_adec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_adec\mpeg_adec_generic.h
# End Source File
# End Group
# Begin Group "mpeg_system"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\mpeg_system\mpeg_es.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_es
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_system\mpeg_ps.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_ps
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_system\mpeg_ts.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_ts

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_ts /D "MODULE_NAME_IS_mpeg_ts"

!ENDIF 

# End Source File
# End Group
# Begin Group "mpeg_vdec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\mpeg_vdec\video_decoder.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_vdec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_vdec\video_decoder.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_vdec\video_parser.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_vdec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_vdec\video_parser.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_vdec\vpar_blocks.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_vdec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_vdec\vpar_blocks.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_vdec\vpar_headers.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_vdec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_vdec\vpar_pool.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_vdec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_vdec\vpar_pool.h
# End Source File
# Begin Source File

SOURCE=..\..\plugins\mpeg_vdec\vpar_synchro.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=mpeg_vdec
# End Source File
# End Group
# Begin Group "network"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\network\ipv4.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=ipv4
# End Source File
# End Group
# Begin Group "sdl"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\sdl\aout_sdl.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=sdl

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=sdl

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\sdl\sdl.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=sdl

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=sdl

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\sdl\vout_sdl.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=sdl

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=sdl

!ENDIF 

# End Source File
# End Group
# Begin Group "spudec"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\spudec\spu_decoder.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=spudec
# End Source File
# Begin Source File

SOURCE=..\..\plugins\spudec\spu_decoder.h
# End Source File
# End Group
# Begin Group "text"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\text\logger.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=logger
# End Source File
# Begin Source File

SOURCE=..\..\plugins\text\rc.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=rc

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=rc

!ENDIF 

# End Source File
# End Group
# Begin Group "vcd"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\vcd\cdrom_tools.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=vcd

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=vcd

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\vcd\cdrom_tools.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\vcd\input_vcd.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=vcd

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=vcd

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\vcd\input_vcd.h

!IF  "$(CFG)" == "vlc - Win32 Release"

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\..\plugins\vcd\vcd.c

!IF  "$(CFG)" == "vlc - Win32 Release"

# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=vcd

!ELSEIF  "$(CFG)" == "vlc - Win32 Debug"

# PROP Exclude_From_Build 1
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=vcd

!ENDIF 

# End Source File
# End Group
# Begin Group "win32"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\plugins\win32\waveout.c
# ADD CPP /D "__BUILTIN__" /D MODULE_NAME=waveout
# End Source File
# End Group
# End Group
# Begin Group "src"

# PROP Default_Filter ""
# Begin Group "audio_output"

# PROP Default_Filter ""
# Begin Source File

SOURCE="..\..\src\audio_output\aout_ext-dec.c"
# End Source File
# Begin Source File

SOURCE=..\..\src\audio_output\aout_pcm.c
# End Source File
# Begin Source File

SOURCE=..\..\src\audio_output\aout_pcm.h
# End Source File
# Begin Source File

SOURCE=..\..\src\audio_output\aout_spdif.c
# End Source File
# Begin Source File

SOURCE=..\..\src\audio_output\aout_spdif.h
# End Source File
# Begin Source File

SOURCE=..\..\src\audio_output\audio_output.c
# End Source File
# End Group
# Begin Group "input"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\input\input.c
# End Source File
# Begin Source File

SOURCE=..\..\src\input\input_clock.c
# End Source File
# Begin Source File

SOURCE=..\..\src\input\input_dec.c
# End Source File
# Begin Source File

SOURCE="..\..\src\input\input_ext-dec.c"
# End Source File
# Begin Source File

SOURCE="..\..\src\input\input_ext-intf.c"
# End Source File
# Begin Source File

SOURCE="..\..\src\input\input_ext-plugins.c"
# End Source File
# Begin Source File

SOURCE=..\..\src\input\input_programs.c
# End Source File
# Begin Source File

SOURCE=..\..\src\input\mpeg_system.c
# End Source File
# End Group
# Begin Group "interface"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\interface\interface.c
# End Source File
# Begin Source File

SOURCE=..\..\src\interface\intf_eject.c
# End Source File
# Begin Source File

SOURCE=..\..\src\interface\intf_msg.c
# End Source File
# Begin Source File

SOURCE=..\..\src\interface\intf_playlist.c
# End Source File
# Begin Source File

SOURCE=..\..\src\interface\main.c
# ADD CPP /I ".."
# End Source File
# End Group
# Begin Group "misc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\misc\configuration.c
# ADD CPP /I ".."
# End Source File
# Begin Source File

SOURCE=..\..\src\misc\iso_lang.c
# End Source File
# Begin Source File

SOURCE=..\..\src\misc\modules.c
# End Source File
# Begin Source File

SOURCE=.\modules_builtin.h
# End Source File
# Begin Source File

SOURCE=..\..\src\misc\modules_plugin.h
# End Source File
# Begin Source File

SOURCE=..\..\src\misc\mtime.c
# End Source File
# Begin Source File

SOURCE=..\..\src\misc\netutils.c
# End Source File
# Begin Source File

SOURCE=..\..\src\misc\win32_specific.c
# End Source File
# End Group
# Begin Group "video_output"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\src\video_output\video_output.c
# End Source File
# Begin Source File

SOURCE=..\..\src\video_output\video_text.c
# ADD CPP /D DATA_PATH=\".\"
# End Source File
# Begin Source File

SOURCE=..\..\src\video_output\video_text.h
# End Source File
# Begin Source File

SOURCE=..\..\src\video_output\vout_pictures.c
# End Source File
# Begin Source File

SOURCE=..\..\src\video_output\vout_subpictures.c
# End Source File
# End Group
# End Group
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\..\include\ac3_downmix.h
# End Source File
# Begin Source File

SOURCE=..\..\include\ac3_imdct.h
# End Source File
# Begin Source File

SOURCE=..\..\include\audio_output.h
# End Source File
# Begin Source File

SOURCE=..\..\include\beos_specific.h
# End Source File
# Begin Source File

SOURCE=..\..\include\common.h
# End Source File
# Begin Source File

SOURCE=..\..\include\config.h
# End Source File
# Begin Source File

SOURCE=..\..\include\configuration.h
# End Source File
# Begin Source File

SOURCE=..\..\include\darwin_specific.h
# End Source File
# Begin Source File

SOURCE=.\defs.h
# End Source File
# Begin Source File

SOURCE="..\..\include\input_ext-dec.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\input_ext-intf.h"
# End Source File
# Begin Source File

SOURCE="..\..\include\input_ext-plugins.h"
# End Source File
# Begin Source File

SOURCE=..\..\include\input_iovec.h
# End Source File
# Begin Source File

SOURCE=..\..\include\interface.h
# End Source File
# Begin Source File

SOURCE=..\..\include\intf_eject.h
# End Source File
# Begin Source File

SOURCE=..\..\include\intf_msg.h
# End Source File
# Begin Source File

SOURCE=..\..\include\intf_playlist.h
# End Source File
# Begin Source File

SOURCE=..\..\include\inttypes.h
# End Source File
# Begin Source File

SOURCE=..\..\include\iso_lang.h
# End Source File
# Begin Source File

SOURCE=..\..\include\main.h
# End Source File
# Begin Source File

SOURCE=..\..\include\mmx.h
# End Source File
# Begin Source File

SOURCE=..\..\include\modules.h
# End Source File
# Begin Source File

SOURCE=..\..\include\modules_inner.h
# End Source File
# Begin Source File

SOURCE=..\..\include\mtime.h
# End Source File
# Begin Source File

SOURCE=..\..\include\netutils.h
# End Source File
# Begin Source File

SOURCE=..\..\include\network.h
# End Source File
# Begin Source File

SOURCE=..\..\include\stream_control.h
# End Source File
# Begin Source File

SOURCE=..\..\include\threads.h
# End Source File
# Begin Source File

SOURCE="..\..\include\vdec_ext-plugins.h"
# End Source File
# Begin Source File

SOURCE=..\..\include\video.h
# End Source File
# Begin Source File

SOURCE=..\..\include\video_output.h
# End Source File
# Begin Source File

SOURCE=..\..\include\videolan\vlc.h
# End Source File
# Begin Source File

SOURCE=..\..\include\win32_specific.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\..\share\vlc_win32_rc.rc

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
