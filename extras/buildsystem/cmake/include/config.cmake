###########################################################
# System Includes
###########################################################
include( CheckIncludeFile )
include (CheckTypeSize)
include (CheckCSourceCompiles)
include (CheckSymbolExists)
include (CheckLibraryExists)

###########################################################
# Options
###########################################################
# Options moved before the custom macro includes because those macros need path values, if the ENABLE_CONTRIB
# has been set.

OPTION( ENABLE_HTTPD           "Enable httpd server" ON )
OPTION( ENABLE_STREAM_OUT      "Enable stream output plugins" ON )
OPTION( ENABLE_VLM             "Enable vlm" ON )
OPTION( ENABLE_DYNAMIC_PLUGINS "Enable dynamic plugin" ON )
OPTION( UPDATE_CHECK           "Enable automatic new version checking" OFF )
OPTION( ENABLE_NO_SYMBOL_CHECK "Don't check symbols of modules against libvlc. (Enabling this option speeds up compilation)" OFF )
OPTION( ENABLE_CONTRIB         "Attempt to use VLC contrib system to get the third-party libraries" ON )
OPTION( ENABLE_LOADER          "Enable the win32 codec loader" OFF )
OPTION( ENABLE_NLS             "Enable translation of the program's messages" ON)

if(ENABLE_CONTRIB)

  set( CONTRIB_INCLUDE ${CMAKE_SOURCE_DIR}/extras/contrib/include)
  set( CONTRIB_LIB ${CMAKE_SOURCE_DIR}/extras/contrib/lib)
  set( CONTRIB_PROGRAM ${CMAKE_SOURCE_DIR}/extras/contrib/bin)
  set( CMAKE_LIBRARY_PATH ${CONTRIB_LIB} ${CMAKE_LIBRARY_PATH} )
  set( CMAKE_PROGRAM_PATH ${CONTRIB_PROGRAM} ${CMAKE_PROGRAM_PATH} )
  set( CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -L${CONTRIB_LIB}" )
  set( CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -L${CONTRIB_LIB}" )
  set( CMAKE_SHARED_MODULE_CREATE_C_FLAGS "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -L${CONTRIB_LIB}" )
  set( CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -L${CONTRIB_LIB}" )
  set( CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS "${CMAKE_SHARED_LIBRARY_CREATE_C_FLAGS} -L${CONTRIB_LIB}" )
  set( CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS} -L${CONTRIB_LIB}" )

  # include extras/contrib/include in the header search pathes
  include_directories(${CONTRIB_INCLUDE})
  set( CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${CONTRIB_INCLUDE} )
  
  # include the extras/contrib/bin to the search path, otherwise, when finding programs it will automatically
  # default to system applications (e.g. we should favor the extras/contrib/bin/pkg-config over the system defined
  # one).
  if(WIN32)
    set( ENV{PATH} "${CONTRIB_PROGRAM};$ENV{PATH}" )
  else(WIN32)
    set( ENV{PATH} "${CONTRIB_PROGRAM}:$ENV{PATH}" )
  endif(WIN32)
endif(ENABLE_CONTRIB)

###########################################################
# Custom Macro Includes
###########################################################

include( ${CMAKE_SOURCE_DIR}/cmake/vlc_check_include_files.cmake )
include( ${CMAKE_SOURCE_DIR}/cmake/vlc_check_functions_exist.cmake )
include( ${CMAKE_SOURCE_DIR}/cmake/vlc_add_compile_flag.cmake )
include( ${CMAKE_SOURCE_DIR}/cmake/vlc_check_type.cmake )
include( ${CMAKE_SOURCE_DIR}/cmake/pkg_check_modules.cmake )

###########################################################
# Versioning
###########################################################

set(VLC_VERSION_MAJOR "0")
set(VLC_VERSION_MINOR "9")
set(VLC_VERSION_PATCH "0")
set(VLC_VERSION_EXTRA "-svn")
set(VLC_VERSION ${VLC_VERSION_MAJOR}.${VLC_VERSION_MINOR}.${VLC_VERSION_PATCH}${VLC_VERSION_EXTRA})

set(PACKAGE "vlc")
set(PACKAGE_NAME "vlc") #for gettext
set(PACKAGE_VERSION "${VLC_VERSION}")
set(PACKAGE_STRING "vlc")
set(VERSION_MESSAGE "vlc-${VLC_VERSION}")
set(COPYRIGHT_MESSAGE "Copyright © the VideoLAN team")
set(COPYRIGHT_YEARS "2001-2008")
set(PACKAGE_VERSION_EXTRA "${VLC_VERSION_EXTRA}")
set(PACKAGE_VERSION_MAJOR "${VLC_VERSION_MAJOR}")
set(PACKAGE_VERSION_MINOR "${VLC_VERSION_MINOR}")
set(PACKAGE_VERSION_REVISION "${VLC_VERSION_PATCH}")

###########################################################
# Preflight Checks
###########################################################

IF (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING  "build type determining compiler flags" FORCE )
endif(NOT CMAKE_BUILD_TYPE )

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUG ON)
    add_definitions(-DDEBUG=1)
    set(NDEBUG OFF)
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

set( HAVE_DYNAMIC_PLUGINS ${ENABLE_DYNAMIC_PLUGINS})
set( LIBEXT ${CMAKE_SHARED_MODULE_SUFFIX})

check_c_source_compiles(
    "struct __attribute__((__packed__)) foo { int a; } b; int main(){return 0;}"
    HAVE_ATTRIBUTE_PACKED)

###########################################################
# Headers checks
###########################################################

vlc_check_include_files (malloc.h stdbool.h locale.h)
vlc_check_include_files (stddef.h stdlib.h sys/stat.h)
vlc_check_include_files (stdio.h stdint.h inttypes.h)
vlc_check_include_files (signal.h unistd.h dirent.h)
vlc_check_include_files (netinet/in.h netinet/udplite.h)
vlc_check_include_files (arpa/inet.h net/if.h)
vlc_check_include_files (netdb.h fcntl.h sys/time.h poll.h)
vlc_check_include_files (errno.h time.h alloca.h)
vlc_check_include_files (limits.h)

vlc_check_include_files (string.h strings.h getopt.h)

vlc_check_include_files (dlfcn.h dl.h)

vlc_check_include_files (kernel/OS.h)
vlc_check_include_files (memory.h)
vlc_check_include_files (mach-o/dyld.h)

vlc_check_include_files (pthread.h)

find_package (Threads)

###########################################################
# Functions/structures checks
###########################################################

set(CMAKE_REQUIRED_LIBRARIES c)
set(CMAKE_EXTRA_INCLUDE_FILES string.h)
vlc_check_functions_exist(strcpy strcasecmp strncasecmp)
vlc_check_functions_exist(strcasestr stristr strdup)
vlc_check_functions_exist(strndup stricmp strnicmp)
vlc_check_functions_exist(atof strtoll atoll lldiv)
vlc_check_functions_exist(strlcpy stristr strnlen strsep)
vlc_check_functions_exist(strtod strtof strtol stroul)
vlc_check_functions_exist(stroull)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES stdio.h)
vlc_check_functions_exist(vasprintf)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES libc.h)
vlc_check_functions_exist(fork)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES stdlib.h)
vlc_check_functions_exist(putenv getenv setenv)
vlc_check_functions_exist(putenv getenv setenv)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES stdio.h)
vlc_check_functions_exist(snprintf asprintf)
vlc_check_functions_exist(putenv getenv setenv)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES unistd.h)
vlc_check_functions_exist(isatty getcwd getuid swab)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES sys/stat.h)
vlc_check_functions_exist(lstat fstat stat)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES arpa/inet.h)
vlc_check_functions_exist(inet_aton inet_ntop inet_pton)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES sys/mman.h)
vlc_check_functions_exist(mmap)
if(HAVE_MMAP)
  vlc_enable_modules(access_mmap)
endif(HAVE_MMAP)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES locale.h)
vlc_check_functions_exist(uselocale)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_REQUIRED_LIBRARIES)

check_library_exists(poll poll "" HAVE_POLL)

check_c_source_compiles(
"#include <langinfo.h>
int main() { char* cs = nl_langinfo(CODESET); }"
HAVE_LANGINFO_CODESET)

vlc_check_type("struct addrinfo" "sys/socket.h;netdb.h")
if(HAVE_STRUCT_ADDRINFO)
  set(HAVE_ADDRINFO ON)
endif(HAVE_STRUCT_ADDRINFO)
vlc_check_type("struct timespec" "time.h")

check_c_source_compiles (
"#include <stdint.h> \n #ifdef UINTMAX \n #error no uintmax
 #endif
 int main() { return 0;}" HAVE_STDINT_H_WITH_UINTMAX)

check_symbol_exists(ntohl "sys/param.h"  NTOHL_IN_SYS_PARAM_H)
check_symbol_exists(scandir "dirent.h"   HAVE_SCANDIR)
check_symbol_exists(localtime_r "time.h" HAVE_LOCALTIME_R)
check_symbol_exists(alloca "alloca.h"    HAVE_ALLOCA)

check_symbol_exists(va_copy "stdarg.h"   HAVE_VACOPY)
check_symbol_exists(__va_copy "stdarg.h" HAVE___VA_COPY)


check_symbol_exists(getnameinfo "sys/types.h;sys/socket.h;netdb.h" HAVE_GETNAMEINFO)
check_symbol_exists(getaddrinfo "sys/types.h;sys/socket.h;netdb.h" HAVE_GETADDRINFO)
if(NOT HAVE_GETADDRINFO)
    check_library_exists(getaddrinfo nsl "" HAVE_GETADDRINFO)
endif(NOT HAVE_GETADDRINFO)

vlc_check_functions_exist(iconv)
if(NOT HAVE_ICONV)
    set(LIBICONV "iconv")
    check_library_exists(iconv iconv "" HAVE_ICONV)
endif(NOT HAVE_ICONV)
set(CMAKE_REQUIRED_LIBRARIES ${LIBICONV})
CHECK_C_SOURCE_COMPILES(" #include <iconv.h>
 int main() { return iconv(0, (char **)0, 0, (char**)0, 0); }" ICONV_NO_CONST)
if( ICONV_NO_CONST )
  set( ICONV_CONST "const" )
else( ICONV_NO_CONST )
  set( ICONV_CONST " ")
endif( ICONV_NO_CONST )
set(CMAKE_REQUIRED_LIBRARIES)

check_library_exists(rt clock_nanosleep "" HAVE_CLOCK_NANOSLEEP)
if (HAVE_CLOCK_NANOSLEEP)
    set(LIBRT "rt")
endif (HAVE_CLOCK_NANOSLEEP)

check_library_exists(m pow "" HAVE_LIBM)
if (HAVE_LIBM)
    set (LIBM "m")
endif (HAVE_LIBM)

check_symbol_exists(connect "sys/types.h;sys/socket.h" HAVE_CONNECT)
if(NOT HAVE_CONNECT)
    check_library_exists(connect socket "" HAVE_CONNECT)
    if(NOT HAVE_CONNECT)
        vlc_module_add_link_libraries(libvlc connect)
        vlc_module_add_link_libraries(cdda   connect)
        vlc_module_add_link_libraries(cddax  connect)
    endif(NOT HAVE_CONNECT)
endif(NOT HAVE_CONNECT)

###########################################################
# Other check
###########################################################

include( ${CMAKE_SOURCE_DIR}/cmake/vlc_test_inline.cmake )

###########################################################
# Platform check
###########################################################

if(APPLE)
    include( ${CMAKE_SOURCE_DIR}/cmake/vlc_find_frameworks.cmake )

    # Mac OS X (10.4 and 10.5)  can't poll a tty properly
    # So we deactivate its uses
    set(HAVE_POLL OFF)

    if(ENABLE_NO_SYMBOL_CHECK)
        set(DYNAMIC_LOOKUP "-undefined dynamic_lookup")
    else(ENABLE_NO_SYMBOL_CHECK)
        set(DYNAMIC_LOOKUP)
    endif(ENABLE_NO_SYMBOL_CHECK)
    set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS
     "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} ${DYNAMIC_LOOKUP}")
    set(CMAKE_SHARED_MODULE_CREATE_C_FLAGS
     "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} ${DYNAMIC_LOOKUP}")

    # For pre-10.5
    set( CMAKE_SHARED_LIBRARY_C_FLAGS "${CMAKE_C_FLAGS} -fno-common")

    set(SYS_DARWIN 1)
    add_definitions(-std=gnu99) # Hack for obj-c files to be compiled with gnu99
    vlc_enable_modules(macosx minimal_macosx opengllayer
                       access_eyetv quartztext auhal)

    # On Pre-10.5
    vlc_module_add_link_flags (ffmpeg "-read_only_relocs warning")

   # vlc_check_include_files (ApplicationServices/ApplicationServices.h)
   # vlc_check_include_files (Carbon/Carbon.h)
   # vlc_check_include_files (CoreAudio/CoreAudio.h)

   # check_symbol_exists (CFLocaleCopyCurrent "CoreFoundation/CoreFoundation.h" "" HAVE_CFLOCALECOPYCURRENT)
   # check_symbol_exists (CFPreferencesCopyAppValue "CoreFoundation/CoreFoundation.h" "" HAVE_CFPREFERENCESCOPYAPPVALUE)

    vlc_find_frameworks(Cocoa Carbon OpenGL AGL IOKit Quicktime
                        WebKit QuartzCore Foundation ApplicationServices
                        CoreAudio AudioUnit AudioToolbox)
    vlc_module_add_link_libraries(macosx
        ${Cocoa_FRAMEWORKS}
        ${IOKit_FRAMEWORKS}
        ${OpenGL_FRAMEWORKS}
        ${AGL_FRAMEWORKS}
        ${Quicktime_FRAMEWORKS}
        ${WebKit_FRAMEWORKS})
    vlc_module_add_link_libraries(minimal_macosx
        ${Cocoa_FRAMEWORKS}
        ${Carbon_FRAMEWORKS}
        ${OpenGL_FRAMEWORKS}
        ${AGL_FRAMEWORKS})
    vlc_module_add_link_libraries(access_eyetv
        ${Foundation_FRAMEWORKS})
    vlc_module_add_link_libraries(opengllayer
         ${Cocoa_FRAMEWORKS}
         ${QuartzCore_FRAMEWORKS}
         ${OpenGL_FRAMEWORKS} )
    vlc_module_add_link_libraries(quartztext
         ${Carbon_FRAMEWORKS}
         ${ApplicationServices_FRAMEWORKS} )
    vlc_module_add_link_libraries(auhal
         ${Carbon_FRAMEWORKS}
         ${CoreAudio_FRAMEWORKS}
         ${AudioUnit_FRAMEWORKS}
         ${AudioToolbox_FRAMEWORKS} )
    vlc_module_add_link_libraries(mp4 ${IOKit_FRAMEWORKS} )
    vlc_module_add_link_libraries(mkv ${IOKit_FRAMEWORKS} )

    add_executable(VLC MACOSX_BUNDLE src/vlc.c)
    target_link_libraries(VLC libvlc)
    set( MacOS ${CMAKE_CURRENT_BINARY_DIR}/VLC.app/Contents/MacOS )
    add_custom_command(
        TARGET VLC
        POST_BUILD
        COMMAND rm -Rf ${CMAKE_CURRENT_BINARY_DIR}/tmp
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/tmp/modules/gui/macosx
        COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/tmp/extras/package/macosx
        COMMAND for i in vlc.xcodeproj Resources README.MacOSX.rtf\; do cp -R ${CMAKE_CURRENT_SOURCE_DIR}/extras/package/macosx/$$i ${CMAKE_CURRENT_BINARY_DIR}/tmp/extras/package/macosx\; done
        COMMAND for i in AUTHORS COPYING THANKS\;do cp ${CMAKE_CURRENT_SOURCE_DIR}/$$i ${CMAKE_CURRENT_BINARY_DIR}/tmp\; done
        COMMAND for i in AppleRemote.h AppleRemote.m about.h about.m applescript.h applescript.m controls.h controls.m equalizer.h equalizer.m intf.h intf.m macosx.m misc.h misc.m open.h open.m output.h output.m playlist.h playlist.m playlistinfo.h playlistinfo.m prefs_widgets.h prefs_widgets.m prefs.h prefs.m vout.h voutqt.m voutgl.m wizard.h wizard.m extended.h extended.m bookmarks.h bookmarks.m sfilters.h sfilters.m update.h update.m interaction.h interaction.m embeddedwindow.h embeddedwindow.m fspanel.h fspanel.m vout.m\; do cp ${CMAKE_CURRENT_SOURCE_DIR}/modules/gui/macosx/$$i ${CMAKE_CURRENT_BINARY_DIR}/tmp/modules/gui/macosx\; done
        COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/tmp/extras/package/macosx && xcodebuild -target vlc | grep -vE '^\([ \\t]|$$\)' && cd ../../../../ && cp ${CMAKE_CURRENT_BINARY_DIR}/tmp/extras/package/macosx/build/Default/VLC.bundle/Contents/Info.plist ${CMAKE_CURRENT_BINARY_DIR}/VLC.app/Contents && cp -R ${CMAKE_CURRENT_BINARY_DIR}/tmp/extras/package/macosx/build/Default/VLC.bundle/Contents/Resources/English.lproj ${CMAKE_CURRENT_BINARY_DIR}/VLC.app/Contents/Resources
        COMMAND cp -r ${CMAKE_CURRENT_SOURCE_DIR}/extras/package/macosx/Resources ${CMAKE_CURRENT_BINARY_DIR}/VLC.app/Contents
        COMMAND find -d ${CMAKE_CURRENT_BINARY_DIR}/VLC.app/Contents/Resources -type d -name \\.svn -exec rm -rf {} "\;"
        COMMAND rm -rf ${MacOS}/modules ${MacOS}/locale ${MacOS}/share
        COMMAND ln -s ${CMAKE_CURRENT_SOURCE_DIR}/share ${MacOS}/share
        COMMAND ln -s ${CMAKE_CURRENT_BINARY_DIR}/modules ${MacOS}/modules
        COMMAND find ${CMAKE_BINARY_DIR}/po -name *.gmo -exec sh -c \"mkdir -p ${MacOS}/locale/\\`basename {}|sed s/\\.gmo//\\`/LC_MESSAGES\; ln -s {} ${MacOS}/locale/\\`basename {}|sed s/\\.gmo//\\`/LC_MESSAGES/vlc.mo\" "\;"
        COMMAND ln -sf VLC ${MacOS}/clivlc #useless?
        COMMAND printf "APPLVLC#" > ${CMAKE_CURRENT_BINARY_DIR}/VLC.app/Contents/PkgInfo
        COMMAND rm -Rf ${CMAKE_CURRENT_BINARY_DIR}/tmp
    )
    set( MacOS )

endif(APPLE)

###########################################################
# info
###########################################################

macro(command_to_configvar command var)
 execute_process(
  COMMAND sh -c "${command}"
  OUTPUT_VARIABLE ${var}
  OUTPUT_STRIP_TRAILING_WHITESPACE)
 string( REPLACE "\n" "\\n" ${var} "${${var}}")
endmacro(command_to_configvar)

command_to_configvar( "whoami" VLC_COMPILE_BY )
command_to_configvar( "hostname" VLC_COMPILE_HOST )
command_to_configvar( "hostname" VLC_COMPILE_DOMAIN )
command_to_configvar( "${CMAKE_C_COMPILER} --version" VLC_COMPILER )
# FIXME: WTF? this is not the configure line!
command_to_configvar( "${CMAKE_C_COMPILER} --version" CONFIGURE_LINE )
set( VLC_COMPILER "${CMAKE_C_COMPILER}" )

###########################################################
# Modules: Following are all listed in options
###########################################################

# This modules are enabled by default but can still be disabled manually
vlc_enable_modules(dummy logger memcpy)
vlc_enable_modules(mpgv mpga m4v m4a h264 vc1 demux_cdg cdg ps pva avi mp4 rawdv rawvid nsv real aiff mjpeg demuxdump flacsys tta)
vlc_enable_modules(cvdsub svcdsub spudec subsdec subsusf t140 dvbsub cc mpeg_audio lpcm a52 dts cinepak flac)
vlc_enable_modules(deinterlace invert adjust transform wave ripple psychedelic gradient motionblur rv32 rotate noise grain extract sharpen)
vlc_enable_modules(converter_fixed mono)
vlc_enable_modules(trivial_resampler ugly_resampler)
vlc_enable_modules(trivial_channel_mixer trivial_mixer)
vlc_enable_modules(playlist export nsc xtag)
vlc_enable_modules(i420_rgb grey_yuv rawvideo blend scale image logo magnify puzzle colorthres)
vlc_enable_modules(wav araw subtitle vobsub adpcm a52sys dtssys au ty voc xa nuv smf)
vlc_enable_modules(access_directory access_file access_udp access_tcp)
vlc_enable_modules(access_http access_mms access_ftp)
vlc_enable_modules(access_filter_bandwidth)
vlc_enable_modules(packetizer_mpegvideo packetizer_h264)
vlc_enable_modules(packetizer_mpeg4video packetizer_mpeg4audio)
vlc_enable_modules(packetizer_vc1)
vlc_enable_modules(spatializer atmo blendbench croppadd)
vlc_enable_modules(asf cmml)
vlc_enable_modules(vmem visual growl_udp)

set(enabled ${ENABLE_STREAM_OUT})
vlc_register_modules(${enabled} access_output_dummy access_output_udp access_output_file access_output_http)
vlc_register_modules(${enabled} mux_ps mux_avi mux_mp4 mux_asf mux_dummy mux_wav mux_mpjpeg)
vlc_register_modules(${enabled} packetizer_copy)

vlc_register_modules(${enabled} stream_out_dummy stream_out_standard stream_out_es stream_out_rtp stream_out_description vod_rtsp)
vlc_register_modules(${enabled} stream_out_duplicate stream_out_display stream_out_transcode stream_out_bridge stream_out_mosaic_bridge stream_out_autodel)
vlc_register_modules(${enabled} stream_out_gather)
# vlc_register_modules(${enabled} stream_out_transrate)
# vlc_register_modules(${enabled} rtcp)
vlc_register_modules(${enabled} profile_parser)

if(NOT mingwce)
   set(enabled ON)
endif(NOT mingwce)
vlc_register_modules(${enabled} access_fake access_filter_timeshift access_filter_record access_filter_dump)
vlc_register_modules(${enabled} gestures rc telnet hotkeys showintf marq podcast shout sap fake folder)
vlc_register_modules(${enabled} rss mosaic wall motiondetect clone crop erase bluescreen alphamask gaussianblur)
vlc_register_modules(${enabled} i420_yuy2 i422_yuy2 i420_ymga i422_i420 yuy2_i422 yuy2_i420 chroma_chain)
vlc_register_modules(${enabled} aout_file linear_resampler bandlimited_resampler)
vlc_register_modules(${enabled} float32_mixer spdif_mixer simple_channel_mixer)
vlc_register_modules(${enabled} dolby_surround_decoder headphone_channel_mixer normvol equalizer param_eq)
vlc_register_modules(${enabled} converter_float a52tospdif dtstospdif audio_format)
set(enabled)

if(NOT WIN32)
   vlc_enable_modules(screensaver signals dynamicoverlay) #motion
endif(NOT WIN32)

# This module is disabled because the CMakeList.txt which
# is generated isn't correct. We'll put that back
# when cmake will be accepted as default build system
vlc_disable_modules(motion)

###########################################################
# libraries
###########################################################

include_directories(${CONTRIB_INCLUDE})

#fixme: use find_package(cddb 0.9.5)
pkg_check_modules(LIBCDDB libcddb>=0.9.5)
if(${LIBCDDB_FOUND})
  vlc_module_add_link_libraries(cdda ${LIBCDDB_LIBRARIES})
  vlc_add_module_compile_flag(cdda ${LIBCDDB_CFLAGS} )
endif(${LIBCDDB_FOUND})

find_package(Dlopen)
set(HAVE_DL_DLOPEN ${Dlopen_FOUND})

# Advanced Linux Sound Architecture (ALSA)
pkg_check_modules(ALSA alsa>=1.0.0-rc4)
if (${ALSA_FOUND})
  set (HAVE_ALSA_NEW_API "1")
else (${ALSA_FOUND})
  pkg_check_modules(ALSA alsa)
endif (${ALSA_FOUND})

if (${ALSA_FOUND})
  set (HAVE_ALSA "1")
  vlc_enable_modules(alsa)
  vlc_add_module_compile_flag(alsa ${ALSA_CFLAGS})
  vlc_module_add_link_libraries(alsa ${ALSA_LIBRARIES})
endif (${ALSA_FOUND})

# Open Sound System (OSS)
# TODO

find_package(FFmpeg)
if(FFmpeg_FOUND)
  string(REPLACE ";" " " FFmpeg_CFLAGS "${FFmpeg_CFLAGS}")
  set( CMAKE_REQUIRED_FLAGS_saved ${CMAKE_REQUIRED_FLAGS} )
  set( CMAKE_REQUIRED_FLAGS ${FFmpeg_CFLAGS} )

  vlc_check_include_files (ffmpeg/avcodec.h libavcodec/avcodec.h)
  vlc_check_include_files (ffmpeg/avutil.h libavutil/avutil.h)
  vlc_check_include_files (ffmpeg/swscale.h libswscale/swscale.h)
  check_c_source_compiles( "#include <stdint.h>\n#include <postproc/postprocess.h>\nint main(){return 0;}" HAVE_POSTPROC_POSTPROCESS_H )
  vlc_check_include_files (libpostproc/postprocess.h)
  
  message( STATUS "avcodec found ${HAVE_FFMPEG_AVCODEC_H} || ${HAVE_LIBAVCODEC_AVCODEC_H}")
  message( STATUS "avutil found ${HAVE_FFMPEG_AVUTIL_H} || ${HAVE_LIBAVUTIL_AVUTIL_H}")
  message( STATUS "swscale found ${HAVE_FFMPEG_SWSCALE_H} || ${HAVE_LIBSWSCALE_SWSCALE_H}")
  message( STATUS "postprocess found ${HAVE_POSTPROC_POSTPROCESS_H} || ${HAVE_LIBPOSTPROC_POSTPROCESS_H}")

  vlc_enable_modules(ffmpeg)
  vlc_add_module_compile_flag(ffmpeg ${FFmpeg_CFLAGS})
  vlc_module_add_link_libraries(ffmpeg ${FFmpeg_LIBRARIES})
  
  set( CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_saved} )
  set( CMAKE_REQUIRED_FLAGS_saved )
endif(FFmpeg_FOUND)

find_package(Lua)
if(Lua_FOUND)
  set(HAVE_LUA TRUE)
  vlc_enable_modules(lua)
  vlc_check_include_files (lua.h lualib.h)
  vlc_add_module_compile_flag(lua ${Lua_CFLAGS} )
  vlc_module_add_link_libraries(lua ${Lua_LIBRARIES})
endif(Lua_FOUND)

find_package(Qt4)
if(QT4_FOUND)
  set(HAVE_QT4 TRUE)
  include_directories(${QT_INCLUDES})
  vlc_check_include_files (qt.h)
  vlc_enable_modules(qt4)
  #execute_process leaves the trailing newline appended to the variable, unlike exec_program
  #execute_process( COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=prefix QtCore OUTPUT_VARIABLE QT4LOCALEDIR)
  exec_program( ${PKG_CONFIG_EXECUTABLE} ARGS --variable=prefix QtCore OUTPUT_VARIABLE QT4LOCALEDIR)
  set(QT4LOCALEDIR ${QT4LOCALEDIR}/share/qt4/translations )
  vlc_add_module_compile_flag(qt4 ${QT_CFLAGS})
  vlc_add_module_compile_flag(qt4 -DQT4LOCALEDIR=\\\\"${QT4LOCALEDIR}\\\\" )
  vlc_module_add_link_libraries(qt4 ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY})

  # Define our own qt4_wrap_ui macro to match wanted behaviour
  MACRO (VLC_QT4_WRAP_UI outfiles )
    FOREACH (it ${ARGN})
     string(REPLACE ".ui" ".h" outfile "${it}")
      GET_FILENAME_COMPONENT(infile ${it} ABSOLUTE)
      SET(outfile ${CMAKE_CURRENT_BINARY_DIR}/${outfile})
      ADD_CUSTOM_COMMAND(OUTPUT ${outfile}
        COMMAND mkdir -p `dirname ${outfile}`
        COMMAND ${QT_UIC_EXECUTABLE}
        ARGS -o ${outfile} ${infile}
        MAIN_DEPENDENCY ${infile})
      SET(${outfiles} ${${outfiles}} ${outfile})
    ENDFOREACH (it)
  ENDMACRO (VLC_QT4_WRAP_UI)

  MACRO (VLC_QT4_GENERATE_MOC outfiles flags )
    string(REGEX MATCHALL "[^\\ ]+" flags_list ${flags})
    FOREACH (it ${ARGN})
      string(REPLACE ".hpp" ".moc.cpp" outfile "${it}")
      GET_FILENAME_COMPONENT(infile ${it} ABSOLUTE)
      SET(outfile ${CMAKE_CURRENT_BINARY_DIR}/${outfile})
      ADD_CUSTOM_COMMAND(OUTPUT ${outfile}
        COMMAND mkdir -p `dirname ${outfile}`
        COMMAND ${QT_MOC_EXECUTABLE} 
        ARGS ${flags_list}
        ARGS -I ${CMAKE_BINARY_DIR}/include
        ARGS -o ${outfile} ${infile}
        MAIN_DEPENDENCY ${it}
        )
      SET(${outfiles} ${${outfiles}} ${outfile})
    ENDFOREACH (it)
  ENDMACRO (VLC_QT4_GENERATE_MOC)


endif(QT4_FOUND)

find_package(OpenGL)
if(OPENGL_FOUND)
  vlc_enable_modules(opengl)
  vlc_check_include_files (gl/gl.h)
  vlc_check_include_files (gl/glu.h)
  vlc_check_include_files (gl/glx.h)
  vlc_add_module_compile_flag(opengl ${OPENGL_CFLAGS})
  vlc_module_add_link_libraries(opengl ${OPENGL_LIBRARIES})
endif(OPENGL_FOUND)

find_package(Matroska 0.7.7)
if(Matroska_FOUND)
  vlc_enable_modules(mkv)
  vlc_check_include_files (matroska/KaxAttachments.h)
  vlc_check_include_files (matroska/KaxVersion.h)
  vlc_module_add_link_libraries(mkv ${Matroska_LIBRARIES})
endif(Matroska_FOUND)

find_package(Live555)
if(Live555_FOUND)
  vlc_enable_modules(live555)
  vlc_add_module_compile_flag(live555 ${Live555_CFLAGS})
  vlc_module_add_link_libraries(live555 ${Live555_LIBRARIES})
endif(Live555_FOUND)

set(CURSES_NEED_NCURSES TRUE)
find_package(Curses)
if(CURSES_LIBRARIES)
  vlc_enable_modules(ncurses)
  vlc_module_add_link_libraries(ncurses ${CURSES_LIBRARIES})
endif(CURSES_LIBRARIES)

find_package(X11)
if(X11_FOUND)
  vlc_enable_modules(x11 panoramix)
  vlc_check_include_files (X11/Xlib.h)
  vlc_module_add_link_libraries(x11       ${X11_LIBRARIES})
  vlc_module_add_link_libraries(panoramix ${X11_LIBRARIES})
endif(X11_FOUND)

vlc_check_include_files (linux/fb.h)
if(HAVE_LINUX_FB_H)
  vlc_enable_modules(fb)
endif(HAVE_LINUX_FB_H)

find_package(Mpeg2)
if(Mpeg2_FOUND)
  vlc_enable_modules(libmpeg2)
  check_include_files ("stdint.h;mpeg2dec/mpeg2.h" HAVE_MPEG2DEC_MPEG2_H)
  vlc_module_add_link_libraries(libmpeg2 ${Mpeg2_LIBRARIES})
endif(Mpeg2_FOUND)

find_package(Dvbpsi)
if(Dvbpsi_FOUND)
  vlc_register_modules(${ENABLE_STREAM_OUT} mux_ts)
  vlc_enable_modules(ts)
  check_include_files ("stdint.h;dvbpsi/dvbpsi.h;dvbpsi/demux.h;dvbpsi/descriptor.h;dvbpsi/pat.h;dvbpsi/pmt.h;dvbpsi/sdt.h;dvbpsi/dr.h" HAVE_DVBPSI_DR_H)
  vlc_module_add_link_libraries(ts      ${Dvbpsi_LIBRARIES})
  vlc_module_add_link_libraries(mux_ts  ${Dvbpsi_LIBRARIES})
  vlc_module_add_link_libraries(dvb     ${Dvbpsi_LIBRARIES})
endif(Dvbpsi_FOUND)

vlc_check_include_files (id3tag.h zlib.h)
if(HAVE_ID3TAG_H AND HAVE_ZLIB_H)
  vlc_enable_modules(id3tag)
  vlc_module_add_link_libraries(id3tag  "id3tag;z")
endif(HAVE_ID3TAG_H AND HAVE_ZLIB_H)

find_package(Taglib)
if(Taglib_FOUND)
  set(HAVE_TAGLIB 1)
  vlc_enable_modules(taglib)
  vlc_module_add_link_libraries(taglib "${Taglib_LIBRARIES};z")
  vlc_add_module_compile_flag(taglib "${Taglib_CFLAGS}")
endif(Taglib_FOUND)

vlc_check_include_files (zlib.h)
if(HAVE_ZLIB_H)
  vlc_module_add_link_libraries(access_http z)
  vlc_module_add_link_libraries(mkv z)
  vlc_module_add_link_libraries(mp4 z)
  vlc_module_add_link_libraries(stream_out_bridge z)
  vlc_module_add_link_libraries(sap z)
endif(HAVE_ZLIB_H)

set(CMAKE_REQUIRED_INCLUDES)

###########################################################
# Final configuration
###########################################################

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/include/config.h)
