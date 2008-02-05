include( CheckIncludeFile )
include (CheckTypeSize)
include (CheckCSourceCompiles)
include (CheckSymbolExists)
include (CheckLibraryExists)
include (FindThreads)

include( ${CMAKE_SOURCE_DIR}/cmake/vlc_check_include_files.cmake )
include( ${CMAKE_SOURCE_DIR}/cmake/vlc_check_functions_exist.cmake )
include( ${CMAKE_SOURCE_DIR}/cmake/vlc_add_compile_flag.cmake )
include( ${CMAKE_SOURCE_DIR}/cmake/vlc_check_type.cmake )
include( ${CMAKE_SOURCE_DIR}/cmake/pkg_check_modules.cmake )

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
vlc_check_include_files (errno.h time.h)

vlc_check_include_files (dlfcn.h dl.h)

vlc_check_include_files (kernel/OS.h)
vlc_check_include_files (mach-o/dyld.h)


###########################################################
# Functions/structures checks
###########################################################

set(CMAKE_EXTRA_INCLUDE_FILES string.h)
vlc_check_functions_exist(strcpy strcasecmp)
vlc_check_functions_exist(strcasestr strdup)
vlc_check_functions_exist(strndup stricmp strnicmp)
vlc_check_functions_exist(atof strtoll atoll lldiv)
vlc_check_functions_exist(strlcpy)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES stdio.h)
vlc_check_functions_exist(vasprintf)
set(CMAKE_EXTRA_INCLUDE_FILES)

set(CMAKE_EXTRA_INCLUDE_FILES libc.h)
vlc_check_functions_exist(fork)
set(CMAKE_EXTRA_INCLUDE_FILES)

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

check_symbol_exists(ntohl "sys/param.h" NTOHL_IN_SYS_PARAM_H)
check_symbol_exists(scandir "dirent.h" HAVE_SCANDIR)
check_symbol_exists(scandir "dirent.h" HAVE_SCANDIR)
check_symbol_exists(localtime_r "time.h" HAVE_LOCALTIME_R)

check_symbol_exists(getnameinfo "sys/types.h;sys/socket.h;netdb.h" HAVE_GETNAMEINFO)
check_symbol_exists(getaddrinfo "sys/types.h;sys/socket.h;netdb.h" HAVE_GETADDRINFO)
if(NOT HAVE_GETADDRINFO)
    check_library_exists(getaddrinfo nsl "" HAVE_GETADDRINFO)
endif(NOT HAVE_GETADDRINFO)

set(LIBICONV "")
vlc_check_functions_exist(iconv)

if(NOT HAVE_ICONV)
    set(LIBICONV "iconv")
    check_library_exists(iconv iconv "" HAVE_ICONV)
endif(NOT HAVE_ICONV)
# FIXME: this will break on *BSD:
set( ICONV_CONST "" )

check_library_exists(rt clock_nanosleep "" HAVE_CLOCK_NANOSLEEP)
if (HAVE_CLOCK_NANOSLEEP)
    set(LIBRT "rt")
endif (HAVE_CLOCK_NANOSLEEP)

check_library_exists(m pow "" HAVE_LIBM)
if (HAVE_LIBM)
    set (LIBM "m")
endif (HAVE_LIBM)

###########################################################
# Other check
###########################################################
include( ${CMAKE_SOURCE_DIR}/cmake/vlc_test_inline.cmake )

###########################################################
# Platform check
###########################################################
if(APPLE)
    include( ${CMAKE_SOURCE_DIR}/cmake/vlc_find_frameworks.cmake )

    set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS
            "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -undefined dynamic_lookup")
        set(CMAKE_SHARED_MODULE_CREATE_C_FLAGS
            "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -undefined dynamic_lookup")

    set(SYS_DARWIN 1)

    vlc_enable_modules(macosx minimal_macosx access_eyetv quartztext)

    vlc_find_frameworks(Cocoa Carbon OpenGL AGL IOKit Quicktime
                        WebKit QuartzCore Foundation ApplicationServices)
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
    vlc_module_add_link_libraries(mp4
         ${IOKit_FRAMEWORKS} )

    add_executable(VLC MACOSX_BUNDLE src/vlc.c)
    target_link_libraries(VLC libvlc)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/VLC.app/Contents/MacOS/modules
        COMMAND ln -s ${CMAKE_CURRENT_BINARY_DIR}/modules ${CMAKE_CURRENT_BINARY_DIR}/VLC.app/Contents/MacOS/lib
    )
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/VLC.app/Contents/MacOS/lib
        COMMAND ln -s ${CMAKE_CURRENT_BINARY_DIR}/modules ${CMAKE_CURRENT_BINARY_DIR}/VLC.app/Contents/MacOS/lib
    )

endif(APPLE)

###########################################################
# version and info setup
###########################################################

macro(command_to_configvar command var)
 execute_process(
  COMMAND sh -c "${command}"
  OUTPUT_VARIABLE ${var}
  OUTPUT_STRIP_TRAILING_WHITESPACE)
 string( REPLACE "\n" "\\n" ${var} ${${var}})
endmacro(command_to_configvar)

command_to_configvar( "whoami" VLC_COMPILE_BY )
command_to_configvar( "hostname" VLC_COMPILE_HOST )
command_to_configvar( "hostname" VLC_COMPILE_DOMAIN )
command_to_configvar( "${CMAKE_C_COMPILER} --version" VLC_COMPILER )
# FIXME: WTF? this is not the configure line!
command_to_configvar( "${CMAKE_C_COMPILER} --version" CONFIGURE_LINE )
set( VLC_COMPILER "${CMAKE_C_COMPILER}" )

set(PACKAGE "vlc")
set(PACKAGE_STRING "vlc")
set(VERSION_MESSAGE "vlc-0.9.0-svn")
set(COPYRIGHT_MESSAGE "Copyright VLC Team")
set(COPYRIGHT_YEARS "2001-2007")

###########################################################
# Options
###########################################################

OPTION( ENABLE_HTTPD           "Enable httpd server" ON )
OPTION( ENABLE_VLM             "Enable vlm" ON )
OPTION( ENABLE_DYNAMIC_PLUGINS "Enable dynamic plugin" ON )

set( HAVE_DYNAMIC_PLUGINS ${ENABLE_DYNAMIC_PLUGINS})
set( LIBEXT ${CMAKE_SHARED_MODULE_SUFFIX})

###########################################################
# Modules: Following are all listed in options
###########################################################

# This module will be enabled but user could disabled it
vlc_enable_modules(dummy logger memcpy)
vlc_enable_modules(mpgv mpga m4v m4a h264 vc1 demux_cdg cdg ps pva avi mp4 rawdv rawvid nsv real aiff mjpeg demuxdump flacsys tta)
vlc_enable_modules(cvdsub svcdsub spudec subsdec subsusf t140 dvbsub cc mpeg_audio lpcm a52 dts cinepak flac)
vlc_enable_modules(deinterlace invert adjust transform wave ripple psychedelic gradient motionblur rv32 rotate noise grain extract sharpen seamcarving)
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
vlc_enable_modules(spatializer)

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
   vlc_enable_modules(screensaver)
endif(NOT WIN32)

# Following modules will be disabled but listed in options

vlc_disable_modules(asf)

# This module is disabled because the CMakeList.txt which
# is generated isn't correct. We'll put that back
# when cmake will be accepted as default build system
vlc_disable_modules(motion)

###########################################################
# libraries
###########################################################

pkg_check_modules(LIBCDDB libcddb>=0.9.5)
if(${LIBCDDB_FOUND})
  vlc_module_add_link_libraries(cdda ${LIBCDDB_LIBRARIES})
  vlc_add_module_compile_flag(cdda ${LIBCDDB_CFLAGS} )
endif(${LIBCDDB_FOUND})

include (FindDlopen)
set(HAVE_DL_DLOPEN ${Dlopen_FOUND})

###########################################################
# Final configuration
###########################################################
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/include/config.h)

