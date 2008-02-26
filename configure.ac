dnl Autoconf settings for vlc

AC_INIT(vlc,0.9.0-svn)
VERSION_MAJOR="0"
VERSION_MINOR="9"
VERSION_REVISION="0"
VERSION_EXTRA="svn"

CONFIGURE_LINE="`echo "$0 $ac_configure_args" | sed -e 's/\\\/\\\\\\\/g'`"
CODENAME="Grishenko"
COPYRIGHT_YEARS="1996-2008"

AC_PREREQ(2.59) dnl this really should be 2.59c
AC_CONFIG_SRCDIR(src/libvlc.c)
AC_CONFIG_AUX_DIR(autotools)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE(tar-ustar)
AM_CONFIG_HEADER(config.h)

dnl Too many people are not aware of maintainer mode:
dnl If you want to use it, you definitely know what you are doing, so
dnl you can specify "--disable-maintainer-mode". But if you want the default
dnl automake behavior, you've likely never heard of maintainer mode, so we
dnl can't expect you to enable it manually.

AS_IF([test "x${enable_maintainer_mode}" != "xno"],
	[enable_maintainer_mode="yes"])
AM_MAINTAINER_MODE

dnl
dnl  Save *FLAGS
dnl
VLC_SAVE_FLAGS

dnl
dnl Check for tools
dnl
AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS
AC_DEFINE([_FORTIFY_SOURCE], 2, [Define to `2' to get glibc warnings.])
AM_PROG_CC_C_O
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_OBJC
_AM_DEPENDENCIES([OBJC])
AC_PROG_EGREP
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AM_PROG_AS

dnl Find the right ranlib, even when cross-compiling
AC_CHECK_TOOL(RANLIB, ranlib, :)
AC_CHECK_TOOL(STRIP, strip, :)
AC_CHECK_TOOL(AR, ar, :)
AC_CHECK_TOOL(LD, ld, :)
AC_CHECK_TOOL(DLLTOOL, dlltool, :)

dnl Check for compiler properties
AC_C_CONST
AC_C_INLINE
AC_C_RESTRICT

dnl
dnl  Check for the contrib directory
dnl
AC_ARG_WITH(contrib,
    [  --without-contrib       do not use the libraries in extras/contrib],[],[])
  if test "${with_contrib}" != "no"; then 
    AC_MSG_CHECKING([for libs in extras/contrib])
    topdir="`dirname $0`"
    if test "`echo \"$topdir\" | cut -c 1`" != "/"; then
       topdir="`pwd`/$topdir"
    fi
   if test "`grep HOST ${topdir}/extras/contrib/config.mak 2>/dev/null|awk '{print $3}'`" != "`$CC -dumpmachine`"; then
      if test "${with_contrib}" = "yes"; then
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([ not using the libs in extras/contrib as it is not the same host])
      else
        AC_MSG_RESULT([no])
        AC_MSG_WARN([ not using the libs in extras/contrib as it is not the same host])
      fi
     else
  if test -d ${topdir}/extras/contrib/lib; then
    AC_MSG_RESULT([yes])
    export PATH=${topdir}/extras/contrib/bin:$PATH
    CPPFLAGS="${CPPFLAGS} -I${topdir}/extras/contrib/include"
    CPPFLAGS_save="${CPPFLAGS_save} -I${topdir}/extras/contrib/include"
    CFLAGS="${CFLAGS} -I${topdir}/extras/contrib/include"
    CFLAGS_save="${CFLAGS_save} -I${topdir}/extras/contrib/include"
    CXXFLAGS="${CXXFLAGS} -I${topdir}/extras/contrib/include"
    CXXFLAGS_save="${CXXFLAGS_save} -I${topdir}/extras/contrib/include"
    OBJCFLAGS="${OBJCFLAGS} -I${topdir}/extras/contrib/include"
    OBJCFLAGS_save="${OBJCFLAGS_save} -I${topdir}/extras/contrib/include"
    if test $build = $host -o "$PKG_CONFIG_LIBDIR"; then
        export PKG_CONFIG_PATH=${topdir}/extras/contrib/lib/pkgconfig:$PKG_CONFIG_PATH
    else
        export PKG_CONFIG_LIBDIR=${topdir}/extras/contrib/lib/pkgconfig
    fi
    LDFLAGS="${LDFLAGS} -L${topdir}/extras/contrib/lib"
    LDFLAGS_save="${LDFLAGS_save} -L${topdir}/extras/contrib/lib"

    if test -z $with_live555_tree; then
       with_live555_tree=${topdir}/extras/contrib/src/live
    fi
    if test -z $with_goom_tree; then
       with_goom_tree=${topdir}/extras/contrib/src/goom
    fi
    if test -z $with_mozilla_sdk_path; then
       with_mozilla_sdk_path=${topdir}/extras/contrib/gecko-sdk
    fi
    if test -z $with_cyberlink_tree; then
       with_cyberlink_tree=${topdir}/extras/contrib/src/clinkcc
    fi
    if test -z $with_libmpeg2_tree; then
       with_libmpeg2_tree=${topdir}/extras/contrib/src/mpeg2dec
    fi
    if test -z $with_a52_tree; then
       with_a52_tree=${topdir}/extras/contrib/src/a52dec
    fi

    if test ".`uname -s`" = ".Darwin"; then
      export LD_LIBRARY_PATH=${topdir}/extras/contrib/lib:$LD_LIBRARY_PATH
      export DYLD_LIBRARY_PATH=${topdir}/extras/contrib/lib:$DYLD_LIBRARY_PATH
    elif test ".`uname -s`" = ".BeOS"; then
      export LIBRARY_PATH=${topdir}/extras/contrib/lib:$LIBRARY_PATH
      export BELIBRARIES=${topdir}/extras/contrib/lib:$BELIBRARIES
    fi
  else
     AC_MSG_RESULT([no])
  fi
  fi
fi
dnl
dnl  Set default values
dnl
LDFLAGS_vlc="${LDFLAGS}"

dnl
dnl  Check the operating system
dnl
case "${host_os}" in
  "")
    SYS=unknown
    ;;
  linux*)
    SYS=linux
    ;;
  bsdi*)
    SYS=bsdi
    CFLAGS_save="${CFLAGS_save} -pthread"; CFLAGS="${CFLAGS_save}"
    VLC_ADD_LIBS([dvd dvdcss vcd cdda vcdx cddax],[-ldvd])
    ;;
  *bsd*)
    SYS="${host_os}"
    CFLAGS_save="${CFLAGS_save} -pthread"; CFLAGS="${CFLAGS_save}"
    case "${host_os}" in
      freebsd*)
        CPPFLAGS_save="${CPPFLAGS_save} -I/usr/local/include"
        CPPFLAGS="${CPPFLAGS_save}"
        LDFLAGS_save="${LDFLAGS_save} -L/usr/local/lib"
        LDFLAGS="${LDFLAGS_save}"
        ;;
    esac
    ;;
  darwin*)
    SYS=darwin
    CFLAGS_save="${CFLAGS_save} -D_INTL_REDIRECT_MACROS"; CFLAGS="${CFLAGS_save}"
    CXXFLAGS_save="${CXXFLAGS_save} -D_INTL_REDIRECT_MACROS"; CXXFLAGS="${CXXFLAGS_save}"
    OBJCFLAGS_save="${OBJCFLAGS_save} -D_INTL_REDIRECT_MACROS -std=gnu99"; OBJCFLAGS="${OBJCFLAGS_save}"
    LDFLAGS_save="${LDFLAGS_save} -Wl,-headerpad_max_install_names"; LDFLAGS="${LDFLAGS_save}"
    VLC_ADD_LDFLAGS([mp4], [-Wl,-framework,IOKit,-framework,CoreFoundation])
    VLC_ADD_LDFLAGS([mkv mp4], [-Wl,-framework,IOKit,-framework,CoreFoundation])
    VLC_ADD_LDFLAGS([vlc],[-Wl,-undefined,dynamic_lookup])
    VLC_ADD_LDFLAGS([libvlc_control],[-Wl,dynamic_lookup])
    VLC_ADD_LDFLAGS([ffmpeg i420_rgb_mmx],[-Wl,-read_only_relocs,suppress])
    VLC_ADD_CFLAGS([motion],[-fconstant-cfstrings])
    VLC_ADD_LDFLAGS([libvlc],[-Wl,-framework,Cocoa,-framework,CoreFoundation])
    VLC_ADD_LDFLAGS([motion],[-Wl,-framework,IOKit,-framework,CoreFoundation])
    AC_ARG_ENABLE(macosx-defaults,
        [  --enable-macosx-defaults Build the default configuration on Mac OS X (default enabled)])
    if test "x${enable_macosx_defaults}" != "xno"
    then
        echo ""
        echo "Building with Mac OS X defaults:"
        with_macosx_version_min="10.4"
        echo "  Assuming --with-macosx-version-min=10.4"
        build_dir=`pwd`
        echo "  Assuming --prefix=${build_dir}/vlc_install_dir"
        ac_default_prefix="${build_dir}/vlc_install_dir"
        enable_macosx="yes"
        echo "  Assuming --enable-macosx"
        enable_faad="yes"
        echo "  Assuming --enable-faad"
        enable_flac="yes"
        echo "  Assuming --enable-flac"
        enable_theora="yes"
        echo "  Assuming --enable-theora"
        enable_shout="yes"
        echo "  Assuming --enable-shout"
        enable_cddax="yes"
        echo "  Assuming --enable-cddax"
        enable_vcdx="yes"
        echo "  Assuming --enable-vcdx"
        enable_caca="yes"
        echo "  Assuming --enable-caca"
        enable_goom="yes"
        echo "  Assuming --enable-goom"
        enable_ncurses="yes"
        echo "  Assuming --enable-ncurses"
        enable_twolame="yes"
        echo "  Assuming --enable-twolame"
        enable_realrtsp="yes"
        echo "  Assuming --enable-realrtsp"

        enable_skins2="no"
        echo "  Assuming --disable-skins2"
        enable_x11="no"
        echo "  Assuming --disable-x11"
        enable_glx="no"
        echo "  Assuming --disable-glx"
        enable_xvideo="no"
        echo "  Assuming --disable-xvideo"

        with_macosx_sdk=""
        echo "  Compiling with default SDK"
    fi
    if test ".`uname -p`" = ".i386"; then
        dnl Due to a ld(64) bug in 10.5 we cannot use our mmx code
        dnl without hacking it a lot, we disable mmx and sse.
        dnl (that bug is about ld being unable to handle
        dnl text relocation)
        save_cflags="$CFLAGS"
        CFLAGS="$CFLAGS -dynamiclib -single_module -read_only_relocs suppress"
        AC_TRY_LINK(
            [int a;], [asm("movq _a,%mm0\n");],
            ac_ld_does_not_support_text_reloc=no,
            ac_ld_does_not_support_text_reloc=yes)
        CFLAGS="$save_cflags"

        if test "x$ac_ld_does_not_support_text_reloc" = "xyes"; then
            enable_mmx="no"
            echo "  Assuming --disable-mmx (due to a bug in ld)"
            enable_sse="no"
            echo "  Assuming --disable-sse (due to a bug in ld)"
        fi
    fi
    dnl
    dnl  Check for Mac OS X SDK settings
    dnl
    AC_ARG_WITH(macosx-sdk,
       [  --with-macosx-sdk=DIR   compile using the SDK in DIR])
    if test "${with_macosx_sdk}" != "" ; then
        CC="${CC} -isysroot ${with_macosx_sdk}" 
        CXX="${CXX} -isysroot ${with_macosx_sdk}"
        OBJC="${OBJC} -isysroot ${with_macosx_sdk}"
        LD="${LD} -syslibroot ${with_macosx_sdk}"
    fi
    AC_ARG_WITH(macosx-version-min,
       [  --with-macosx-version-min=VERSION  compile for MacOSX VERSION and above])
    if test "${with_macosx_version_min}" != "" ; then
        CC="${CC} -mmacosx-version-min=${with_macosx_version_min}" 
        CXX="${CXX} -mmacosx-version-min=${with_macosx_version_min}"
        OBJC="${OBJC} -mmacosx-version-min=${with_macosx_version_min}"
        LD="${LD} -macosx_version_min=${with_macosx_version_min}"
        CFLAGS_save="${CFLAGS_save} -DMACOSX_DEPLOYMENT_TARGET=${with_macosx_version_min}"; CFLAGS="${CFLAGS_save}"
        CXXFLAGS_save="${CXXFLAGS_save} -DMACOSX_DEPLOYMENT_TARGET=${with_macosx_version_min}"; CXXFLAGS="${CXXFLAGS_save}"
        OBJCFLAGS_save="${OBJCFLAGS_save} -DMACOSX_DEPLOYMENT_TARGET=${with_macosx_version_min}"; OBJCFLAGS="${OBJCFLAGS_save}"
        MACOSX_DEPLOYMENT_TARGET=${with_macosx_version_min}
        export MACOSX_DEPLOYMENT_TARGET
    fi
    ;;
  *mingw32* | *cygwin* | *wince* | *mingwce* | *pe*)
    AC_CHECK_TOOL(WINDRES, windres, :)

    case "${host_os}" in
      *mingw32*)
        SYS=mingw32
        ;;
      *cygwin*)
        dnl Check if we are using the mno-cygwin mode in which case we are
        dnl actually dealing with a mingw32 compiler.
        AC_EGREP_CPP(yes,
            [#ifdef WIN32
             yes
             #endif],
            SYS=mingw32, SYS=cygwin)
        ;;
      *wince* | *mingwce* | *pe*)
        SYS=mingwce
        ;;
    esac

    if test "${SYS}" = "mingw32"; then
        # add ws2_32 for closesocket, select, recv
        VLC_ADD_LIBS([libvlc],[-lws2_32 -lnetapi32 -lwinmm])
        VLC_ADD_LDFLAGS([vlc],[-mwindows])
        VLC_ADD_LIBS([activex mozilla],[-lgdi32])
        VLC_ADD_LIBS([cdda vcdx cddax sdl_image],[-lwinmm])
        VLC_ADD_LIBS([access_http access_mms access_udp access_tcp access_ftp access_output_udp access_output_shout sap slp http stream_out_standard stream_out_rtp vod_rtsp access_realrtsp telnet rc netsync gnutls growl_udp flac ts audioscrobbler lua],[-lws2_32])
    fi
    if test "${SYS}" = "mingwce"; then
        # add ws2 for closesocket, select, recv
        VLC_ADD_CPPFLAGS([libvlc vlc],[-Dmain(a,b)=maince(a,b)])
        VLC_ADD_LDFLAGS([libvlc vlc],[-e WinMainCRTStartup])
        VLC_ADD_LIBS([libvlc access_http access_mms access_udp access_tcp access_ftp access_output_udp sap http netsync],[-lws2])
   fi
    ;;
  *nto*)
    SYS=nto
    VLC_ADD_LIBS([x11 xvideo],[-lsocket])
    ;;
  solaris*)
    SYS=solaris
    # _POSIX_PTHREAD_SEMANTICS is needed to get the POSIX ctime_r
    # Perhaps it is useful other places as well? 
    CFLAGS_save="${CFLAGS_save} -D_POSIX_PTHREAD_SEMANTICS"; CFLAGS="${CFLAGS_save}"
    ;;
  hpux*)
    SYS=hpux
    ;;
  beos)
    SYS=beos
    CFLAGS_save="${CFLAGS_save} -Wno-multichar"; CFLAGS="${CFLAGS_save}"
    CXXFLAGS_save="${CXXFLAGS_save} -Wno-multichar"; CXXFLAGS="${CXXFLAGS_save}"
    VLC_ADD_CXXFLAGS([beos],[])
    VLC_ADD_LIBS([vlc libvlc logger],[-lbe])
    VLC_ADD_LIBS([beos],[-lbe -lmedia -ltranslation -ltracker -lgame])
    VLC_ADD_LIBS([dvdnav dvdread],[-ldl])
    VLC_ADD_LIBS([access_file],[-lpoll])
    LDFLAGS_save="${LDFLAGS_save} -lintl"; LDFLAGS="${LDFLAGS_save}"
    dnl Check for BONE
    if test -f /boot/beos/system/lib/libbind.so; then
        VLC_ADD_LIBS([access_file access_ftp access_mms access_output_udp telnet netsync sap libvlc growl_udp],[-lbind -lsocket])
    else
        VLC_ADD_LIBS([access_file access_ftp access_mms access_output_udp telnet netsync sap libvlc growl_udp],[-lnet])
    fi

    dnl Ugly check for Zeta
    if test -f /boot/beos/system/lib/libzeta.so; then
        VLC_ADD_LIBS([beos],[-lzeta])
    fi
    ;;
  *)
    SYS="${host_os}"
    ;;
esac
AM_CONDITIONAL(HAVE_BEOS, test "${SYS}" = "beos")
AM_CONDITIONAL(HAVE_DARWIN, test "${SYS}" = "darwin")
AM_CONDITIONAL(HAVE_WIN32, test "${SYS}" = "mingw32")
AM_CONDITIONAL(HAVE_WINCE, test "${SYS}" = "mingwce")

dnl
dnl Sadly autoconf doesn't think about testing foo.exe when ask to test 
dnl for program foo on win32

case "${build_os}" in
    cygwin|msys)
        ac_executable_extensions=".exe"
    ;;
    *)
    ;;
esac

dnl 
dnl  Libtool
dnl  It's very bad, but our former custom system was worst
dnl  -- Courmisch
dnl

dnl override platform specific check for dependent libraries
dnl otherwise libtool linking of shared libraries will
dnl fail on anything other than pass_all.
AC_CACHE_VAL(lt_cv_deplibs_check_method,
    [lt_cv_deplibs_check_method=pass_all])

AC_DISABLE_STATIC
AC_DISABLE_FAST_INSTALL
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
m4_undefine([AC_PROG_F77])
m4_defun([AC_PROG_F77],[])
AC_PROG_LIBTOOL

m4_undefine([AC_DEPLIBS_CHECK_METHOD])
m4_defun([AC_DEPLIBS_CHECK_METHOD],[])

lt_cv_deplibs_check_method=pass_all


dnl
dnl Gettext stuff
dnl
AM_GNU_GETTEXT_VERSION(0.16.1)
AM_GNU_GETTEXT([external])
VLC_ADD_LIBS([libvlc vlc], [${LTLIBINTL}])

dnl
dnl Iconv stuff
dnl
AM_ICONV
AS_IF([test "$am_cv_func_iconv" != "yes"],
	[AC_MSG_ERROR([libiconv is needed for VLC to work properly])])
VLC_ADD_CFLAGS([libvlc],[${INCICONV}])
VLC_ADD_LIBS([libvlc],[${LTLIBICONV}])

dnl Check for the need to include the mingwex lib for mingw32
if test "${SYS}" = "mingw32"
then
    AC_CHECK_LIB(mingwex,opendir,
        AC_CHECK_LIB(mingw32,opendir,,
            [VLC_ADD_LIBS([libvlc gtk],[-lmingwex])])
    )
fi

dnl Check for fnative-struct or mms-bitfields support for mingw32
if test "${SYS}" = "mingw32"
then
    AC_CACHE_CHECK([if \$CC accepts -mms-bitfields],
        [ac_cv_c_mms_bitfields],
        [CFLAGS="${CFLAGS_save} -mms-bitfields"
         AC_TRY_COMPILE([],,ac_cv_c_mms_bitfields=yes, ac_cv_c_mms_bitfields=no)])
    if test "${ac_cv_c_mms_bitfields}" != "no"; then
        CFLAGS_save="${CFLAGS_save} -mms-bitfields";
        CXXFLAGS_save="${CXXFLAGS_save} -mms-bitfields";
    else
        AC_CACHE_CHECK([if \$CC accepts -fnative-struct],
            [ac_cv_c_fnative_struct],
            [CFLAGS="${CFLAGS_save} -fnative-struct"
             AC_TRY_COMPILE([],,ac_cv_c_fnative_struct=yes, ac_cv_c_fnative_struct=no)])
        if test "${ac_cv_c_fnative_struct}" != "no"; then
            CFLAGS_save="${CFLAGS_save} -fnative-struct";
            CXXFLAGS_save="${CXXFLAGS_save} -fnative-struct";
        fi
    fi

    CFLAGS="${CFLAGS_save}"; CXXFLAGS="${CXXFLAGS_save}"
fi

dnl Check for fvtable-thunks support for mingw32
if test "${SYS}" = "mingw32" -a "${CXX}" != ""
then
AC_LANG_PUSH(C++)
    AC_CACHE_CHECK([if \$CXX accepts -fvtable-thunks],
        [ac_cv_cxx_fvtable_thunks],
        [CXXFLAGS="${CXXFLAGS_save} -Wall -Werror -fvtable-thunks"
         AC_TRY_COMPILE([],,ac_cv_cxx_fvtable_thunks=yes,
                        ac_cv_cxx_fvtable_thunks=no)])
    if test "${ac_cv_cxx_fvtable_thunks}" = "yes"; then
        CXXFLAGS_mingw32_special="-fvtable-thunks"
    fi

    CXXFLAGS_save="${CXXFLAGS_save} ${CXXFLAGS_mingw32_special}"; CXXFLAGS="${CXXFLAGS_save}"
AC_LANG_POP(C++)
fi

dnl Plugin compilation stuff

VLC_LIBRARY_SUFFIX

VLC_SYMBOL_PREFIX
AC_SUBST(SYMPREF)

dnl The -DSYS_FOO flag
CPPFLAGS_save="${CPPFLAGS_save} -DSYS_`echo ${SYS} | sed -e 's/-.*//' | tr 'abcdefghijklmnopqrstuvwxyz.' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ_'`"; CPPFLAGS="${CPPFLAGS_save}"

dnl Check for system libs needed
need_libc=false

dnl Check for usual libc functions
AC_CHECK_FUNCS([gettimeofday strtod strtol strtof strtoll strtoull strsep isatty vasprintf asprintf swab sigrelse getpwuid memalign posix_memalign if_nametoindex atoll getenv putenv setenv gmtime_r ctime_r localtime_r lrintf daemon scandir fork bsearch lstat strlcpy strdup strndup strnlen atof lldiv posix_fadvise posix_madvise])
AC_CHECK_FUNCS(strcasecmp,,[AC_CHECK_FUNCS(stricmp)])
AC_CHECK_FUNCS(strncasecmp,,[AC_CHECK_FUNCS(strnicmp)])
AC_CHECK_FUNCS(strcasestr,,[AC_CHECK_FUNCS(stristr)])
AC_FUNC_ALLOCA

AH_BOTTOM([#include <vlc_fixups.h>])

AC_CHECK_FUNCS(mmap, [VLC_ADD_PLUGINS([access_mmap])])

AS_IF([test "${SYS}" = "linux"], [
  AC_CHECK_FUNCS(tee)
])

dnl Check for setlocal and langinfo
AC_CHECK_FUNCS(setlocale)
AC_CHECK_HEADERS(langinfo.h)
AC_CHECK_FUNCS(nl_langinfo)

AC_CACHE_CHECK([for nl_langinfo and CODESET], ac_cv_langinfo_codeset,
  [AC_TRY_LINK([#include <langinfo.h>],
    [char* cs = nl_langinfo(CODESET);],
    ac_cv_langinfo_codeset=yes,
    ac_cv_langinfo_codeset=no)
  ])
if test ${ac_cv_langinfo_codeset} = yes; then
  AC_DEFINE(HAVE_LANGINFO_CODESET, 1,
    [Define if you have <langinfo.h> and nl_langinfo(CODESET).])
fi

AC_CHECK_FUNCS(connect,,[
  AC_CHECK_LIB(socket,connect,[
    VLC_ADD_LIBS([libvlc cdda cddax],-lsocket)
    LIBS_socket="-lsocket"
  ])
])

AC_CHECK_FUNCS(send,,[
  AC_CHECK_LIB(socket,send,[
    VLC_ADD_LIBS([access_http access_mms access_udp access_tcp access_ftp sap access_output_udp stream_out_standard growl_udp],[-lsocket])
  ])
])

AC_CHECK_FUNCS(gethostbyname,,[
  AC_CHECK_LIB(nsl,gethostbyname,[
    VLC_ADD_LIBS([cdda cddax libvlc],[-lnsl])
  ],[
    AC_CHECK_LIB(bind,gethostbyname,[
      VLC_ADD_LIBS([access_mms],[-lbind])
    ])
  ])
])

AC_CHECK_HEADERS(zlib.h, [
  VLC_ADD_LIBS([access_http],[-lz])
])

dnl Check for socklen_t
AH_TEMPLATE(socklen_t, [Define to `int' if <sys/socket.h> does not define.])
AC_CACHE_CHECK([for socklen_t in sys/socket.h],
ac_cv_type_socklen_t,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#include <sys/types.h>
#ifdef WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <sys/socket.h>
#endif]], [[socklen_t len; len = 0;]])],
ac_cv_type_socklen_t=yes,
ac_cv_type_socklen_t=no)])
AS_IF([test "$ac_cv_type_socklen_t" = no],
 [AC_DEFINE(socklen_t, int)])

dnl Check for struct sockaddr_storage
AH_TEMPLATE(sockaddr_storage, [Define to `sockaddr' if <sys/socket.h> does not define.])
AH_TEMPLATE(ss_family, [Define to `sa_family' if <sys/socket.h> does not define.])
AC_CACHE_CHECK([for struct sockaddr_storage], ac_cv_struct_sockaddr_storage,
  [AC_TRY_COMPILE(
    [#include <sys/types.h>
     #if defined( UNDER_CE )
     # include <winsock.h>
     #elif defined( WIN32 )
     # include <winsock2.h>
     #else
     # include <sys/socket.h>
     #endif], [struct sockaddr_storage addr;],
     ac_cv_struct_sockaddr_storage=yes,
     ac_cv_struct_sockaddr_storage=no)])
AS_IF([test "${ac_cv_struct_sockaddr_storage}" = no], [
  AC_DEFINE(sockaddr_storage, sockaddr)
  AC_DEFINE(ss_family, sa_family)
])

dnl getaddrinfo, getnameinfo and gai_strerror check
dnl  -lnsl and -lsocket are needed on Solaris;
dnl  we purposedly make the test fail on Windows
LIBS_save="${LIBS}"
AH_TEMPLATE(HAVE_GETADDRINFO, [Define to 1 if you have the `getaddrinfo' function.])
AC_SEARCH_LIBS([getaddrinfo], [nsl], [AC_DEFINE(HAVE_GETADDRINFO)],, [${LIBS_socket}])

dnl NOTE: we assume getaddrinfo will be present if getnameinfo or gai_strerro
dnl are
LIBS="${LIBS_gai}"
AC_CHECK_FUNCS([getnameinfo gai_strerror])
LIBS="${LIBS_save}"

AH_TEMPLATE(HAVE_ADDRINFO, [Define to 1 if <netdb.h> defines `struct addrinfo'.])
AC_CHECK_TYPES([struct addrinfo],[AC_DEFINE(HAVE_ADDRINFO)],,
[#include <sys/types.h>
#if defined( WIN32 ) || defined( UNDER_CE )
# if defined(UNDER_CE) && defined(sockaddr_storage)
#  undef sockaddr_storage
# endif
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <netdb.h>
#endif])

dnl Check for va_copy
AC_CACHE_CHECK([for va_copy], ac_cv_c_va_copy,
  AC_TRY_LINK(
    [#include <stdarg.h>],
    [va_list ap1, ap2; va_copy(ap1,ap2);],
    [ac_cv_c_va_copy="yes"],
    [ac_cv_c_va_copy="no"]))
if test "${ac_cv_c_va_copy}" = "yes"; then
  AC_DEFINE(HAVE_VA_COPY, 1, [Define if <stdarg.h> defines va_copy.])
fi
AC_CACHE_CHECK([for __va_copy], ac_cv_c___va_copy,
  AC_TRY_LINK(
    [#include <stdarg.h>],
    [va_list ap1, ap2; __va_copy(ap1,ap2);],
    [ac_cv_c___va_copy="yes"],
    [ac_cv_c___va_copy="no"]))
if test "${ac_cv_c___va_copy}" = "yes"; then
  AC_DEFINE(HAVE___VA_COPY, 1, [Define if <stdarg.h> defines __va_copy.])
fi

AC_CHECK_FUNCS(inet_aton,,[
  AC_CHECK_LIB(resolv,inet_aton,[
    VLC_ADD_LIBS([libvlc],[-lresolv])
  ])
])

dnl Check for getopt (always use builtin one on win32)
if test "${SYS}" = "mingw32" -o "${SYS}" = "mingwce"; then
need_getopt=:
else
need_getopt=false
AC_CHECK_FUNCS(getopt_long,[AC_DEFINE(HAVE_GETOPT_LONG,1,long getopt support)],
[ # FreeBSD has a gnugetopt library for this:
  AC_CHECK_LIB([gnugetopt],[getopt_long],
    [AC_DEFINE(HAVE_GETOPT_LONG,1,getopt support)
     VLC_ADD_LIBS([libvlc],[-lgnugetopt])],
    [need_getopt=:])])
fi
AM_CONDITIONAL(BUILD_GETOPT, ${need_getopt})

if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
AC_TYPE_SIGNAL
AC_CHECK_LIB(m,cos,[
  VLC_ADD_LIBS([adjust wave ripple psychedelic gradient a52tofloat32 dtstofloat32 x264 goom visual panoramix rotate noise grain],[-lm])
])
AC_CHECK_LIB(m,pow,[
  VLC_ADD_LIBS([ffmpeg ffmpegaltivec stream_out_transrate i420_rgb faad twolame equalizer spatializer param_eq libvlc vorbis freetype mod mpc dmo quicktime realaudio galaktos opengl],[-lm])
])
AC_CHECK_LIB(m,sqrt,[
  VLC_ADD_LIBS([headphone_channel_mixer normvol speex mono colorthres extract],[-lm])
])
AC_CHECK_LIB(m,ceil,[
  VLC_ADD_LIBS([mosaic],[-lm])
])
AC_CHECK_LIB(m,exp,[
  VLC_ADD_LIBS([gaussianblur],[-lm])
])
AC_CHECK_LIB(mx,sqrtf,[
  VLC_ADD_LIBS([x264],[-lmx])
])
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

dnl Check for dynamic plugins
ac_cv_have_plugins=no

# OS X style
AC_CHECK_HEADERS(mach-o/dyld.h,
  [AC_CHECK_FUNCS(NSLinkModule,
    [AC_DEFINE(HAVE_DL_DYLD, 1, [Define if you have the Darwin dyld API])
     ac_cv_have_plugins=yes])])

# HP-UX style
if test "${ac_cv_have_plugins}" = "no"; then
  AC_CHECK_HEADERS(dl.h)
  ac_cv_my_have_shl_load=no
  AC_CHECK_FUNC(shl_load,
   [ac_cv_my_have_shl_load=yes,
    AC_CHECK_LIB(dld, shl_load,
     [ac_cv_my_have_shl_load=yes
      VLC_ADD_LIBS([libvlc],[-ldld])])])
  if test "${ac_cv_my_have_shl_load}" = "yes"; then
    AC_DEFINE(HAVE_DL_SHL_LOAD, 1, [Define if you have the shl_load API])
    ac_cv_have_plugins=yes
  fi
fi

# Whatever style
if test "${ac_cv_have_plugins}" = "no"; then
  AC_CHECK_LIB(dld, dld_link,
   [VLC_ADD_LIBS([libvlc],[-ldld])
    AC_DEFINE(HAVE_DL_DLD_LINK, 1, [Define if you have the GNU dld library])
    ac_cv_have_plugins=yes])
fi

# Win32 style
if test "${ac_cv_have_plugins}" = "no"; then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"; then
    AC_CHECK_LIB(kernel32, main,
     [VLC_ADD_LIBS([libvlc],[-lkernel32])
      AC_DEFINE(HAVE_DL_WINDOWS, 1, [Define if you have Windows' LoadLibrary])
      ac_cv_have_plugins=yes])
  fi
fi

# WinCE style
if test "${ac_cv_have_plugins}" = "no"; then
  if test "${SYS}" = "mingwce"; then
    AC_DEFINE(HAVE_DL_WINDOWS, 1, [Define if you have Windows' LoadLibrary])
    ac_cv_have_plugins=yes
  fi
fi

# BeOS style
if test "${ac_cv_have_plugins}" = "no"; then
  AC_CHECK_HEADERS(image.h)
  AC_CHECK_FUNCS(load_add_on,
   [AC_DEFINE(HAVE_DL_BEOS, 1, [Define if you have the BeOS dl])
    ac_cv_have_plugins=yes])
fi

# Only test for dlopen() if the others didn't work
if test "${ac_cv_have_plugins}" = "no"; then
  AC_CHECK_HEADERS(dlfcn.h sys/dl.h)
  ac_cv_my_have_dlopen=no
  AC_CHECK_FUNC(dlopen,
    ac_cv_my_have_dlopen=yes,
    AC_CHECK_LIB(dl, dlopen,
      ac_cv_my_have_dlopen=yes
      VLC_ADD_LIBS([libvlc realaudio],[-ldl]),
      AC_CHECK_LIB(svld, dlopen,
        ac_cv_my_have_dlopen=yes
        VLC_ADD_LIBS([libvlc],[-lsvld]))))
  if test "${ac_cv_my_have_dlopen}" = "yes"; then
    AC_DEFINE(HAVE_DL_DLOPEN, 1, [Define if you have the dlopen API])
    ac_cv_have_plugins=yes
  fi
fi

if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
dnl Check for pthreads - borrowed from XMMS
THREAD_LIB=error
if test "${THREAD_LIB}" = "error"; then
  AC_CHECK_LIB(pthread,main,THREAD_LIB="-lpthread")
fi
if test "${THREAD_LIB}" = "error"; then
  AC_CHECK_LIB(pthreads,main,THREAD_LIB="-lpthreads")
fi
if test "${THREAD_LIB}" = "error"; then
  AC_CHECK_LIB(c_r,main,THREAD_LIB="-lc_r")
fi
if test "${THREAD_LIB}" = "error"; then
  AC_CHECK_FUNCS(pthread_mutex_lock)
  THREAD_LIB=""
fi

VLC_ADD_LIBS([libvlc plugin],[${THREAD_LIB}])

AC_CHECK_LIB(rt, clock_nanosleep, [
  VLC_ADD_LIBS([libvlc],[-lrt])
  AC_DEFINE(HAVE_CLOCK_NANOSLEEP, 1, [Define to 1 if you have clock_nanosleep.])
], [
  dnl HP/UX port
  AC_CHECK_LIB(rt,sem_init, [VLC_ADD_LIBS([libvlc],[-lrt])])
])

have_nanosleep=false
AC_CHECK_FUNCS(nanosleep,have_nanosleep=:,[
  AC_CHECK_LIB(rt,nanosleep,
    [VLC_ADD_LIBS([libvlc],[-lrt]) have_nanosleep=:],
    [AC_CHECK_LIB(posix4,nanosleep,
        [VLC_ADD_LIBS([libvlc],[-lposix4]) have_nanosleep=:])]
  )
])
if ${have_nanosleep}; then
  AC_DEFINE(HAVE_NANOSLEEP, 1, [Define if nanosleep is available.])
fi
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" 

dnl Check for misc headers
AC_MSG_CHECKING(for strncasecmp in strings.h)
AC_EGREP_HEADER(strncasecmp,strings.h,[
  AC_MSG_RESULT(yes)
  AC_DEFINE(STRNCASECMP_IN_STRINGS_H, 1,
            Define if <strings.h> defines strncasecmp.)],[
  AC_MSG_RESULT(no)])

dnl Check for headers
AC_CHECK_HEADERS(signal.h time.h errno.h stdint.h stdbool.h getopt.h strings.h inttypes.h sys/int_types.h wchar.h locale.h)
AC_CHECK_HEADERS(sys/sockio.h fcntl.h sys/types.h sys/time.h sys/times.h sys/ioctl.h sys/stat.h)
AC_CHECK_HEADERS([arpa/inet.h netinet/in.h netinet/udplite.h])
AC_CHECK_HEADERS([net/if.h], [], [],
  [
    #include <sys/socket.h>
  ])
if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
AC_CHECK_HEADERS(machine/param.h sys/shm.h)
AC_CHECK_HEADERS([linux/version.h linux/dccp.h])
AC_CHECK_HEADERS(syslog.h)
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

AC_HEADER_TIME

dnl LP64 adn LLP64 architectures had better define ssize_t by themselves...
AH_TEMPLATE(ssize_t, [Define to `int' if <stddef.h> does not define.])
AC_CHECK_TYPE(ssize_t,, [
  AC_DEFINE(ssize_t, int)
])

dnl don't attemp to use poll on darwin, it can't poll a tty. select will do a better job.
if test "${SYS}" != "darwin"; then
  AC_SEARCH_LIBS(poll, [poll], [AC_DEFINE(HAVE_POLL, 1, [Define to 1 if the OS is usabl... err, has poll().])])
fi

dnl Check for dirent
need_dirent=false
AC_CHECK_HEADERS(dirent.h,,[need_dirent=:])
AM_CONDITIONAL(BUILD_DIRENT, ${need_dirent})

dnl Mac OS X and other OSes don't have declaration for nanosleep
if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
AC_MSG_CHECKING(for nanosleep in time.h)
AC_EGREP_HEADER(nanosleep,time.h,[
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_DECL_NANOSLEEP, 1,
            Define if <time.h> defines nanosleep.)
],[
  AC_MSG_RESULT(no)
])
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

dnl Make sure we have timespecs
AC_MSG_CHECKING(for timespec in sys/time.h)
AC_EGREP_HEADER(timespec,sys/time.h,[
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_STRUCT_TIMESPEC, 1,
            Define if <sys/time.h> defines struct timespec.)
],[
  AC_MSG_RESULT(no)
])

dnl Check for threads library
if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
AC_CHECK_HEADERS(pthread.h kernel/scheduler.h kernel/OS.h)
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

dnl Find where are the X headers and libraries
AC_PATH_XTRA()

dnl it seems that autoconf do the pkg-config detection only for the first PKG_CHECK_MODULES in the configure.ac ( which is logical) but in our case it is nested in a if so it was not working if you're not on linux or have disable hal.

PKG_PROG_PKG_CONFIG()

dnl Check for hal
AC_ARG_ENABLE(hal,
  [  --enable-hal            Linux HAL services discovery (default enabled)])
 
if test "${enable_hal}" != "no"
then
  PKG_CHECK_MODULES(HAL, hal >= 0.5.0,
    [ AC_DEFINE( HAVE_HAL_1, 1 , [Define if you have the new HAL library API])
      AC_DEFINE( HAVE_HAL, 1, [Define if you have the HAL library] )
      VLC_ADD_PLUGINS([hal probe_hal]) 
      VLC_ADD_LIBS([libvlc hal probe_hal],[$HAL_LIBS])
      VLC_ADD_CFLAGS([libvlc hal probe_hal],[$HAL_CFLAGS])],   
    dnl No hal 0.5 Test for 0.2
    [ PKG_CHECK_MODULES( HAL, hal >= 0.2.97, 
       [AC_DEFINE(HAVE_HAL, 1, [Define if you have the HAL library])
        VLC_ADD_PLUGINS([hal])
        VLC_ADD_LIBS([libvlc hal],[$HAL_LIBS])
        VLC_ADD_CFLAGS([libvlc hal],[$HAL_CFLAGS])],
       [AC_MSG_WARN(HAL library not found)])
    ]
  )
fi

dnl Check for dbus
AC_ARG_ENABLE(dbus,
  [  --enable-dbus           Linux D-BUS message bus system (default enabled)])
case "${SYS}" in
    linux*|*bsd*)
if test "${enable_dbus}" != "no"
then
  dnl api stable dbus
  PKG_CHECK_MODULES(DBUS, dbus-1 >= 1.0.0,
    [ AC_DEFINE( HAVE_DBUS_3, 1, [Define if you have the D-BUS library API >= 1.0.0] )
      AC_DEFINE( HAVE_DBUS_2, 1, [Define if you have the D-BUS library API >= 0.92] )
      AC_DEFINE( HAVE_DBUS_1, 1, [Define if you have the D-BUS library API >= 0.30] )
      AC_DEFINE( HAVE_DBUS, 1, [Define if you have the D-BUS library] )
      VLC_ADD_LIBS([screensaver libvlc],[$DBUS_LIBS])
      VLC_ADD_CFLAGS([screensaver libvlc],[$DBUS_CFLAGS])
      dnl Check for dbus control interface
        AC_ARG_ENABLE(dbus-control, [  --disable-dbus-control  D-BUS control interface (default enabled)])
        if test "${enable_dbus_control}" != "no"
        then
          VLC_ADD_PLUGINS([dbus])
          VLC_ADD_LIBS([dbus],[$DBUS_LIBS])
          VLC_ADD_CFLAGS([dbus],[$DBUS_CFLAGS])
        fi
      dnl Check for Telepathy
        AC_ARG_ENABLE(telepathy, [  --enable-telepathy      Telepathy Presence plugin through DBus(default enabled)])
        if test "${enable_telepathy}" != "no"; then
          VLC_ADD_PLUGINS([telepathy])
          VLC_ADD_LIBS([telepathy],[$DBUS_LIBS])
          VLC_ADD_CFLAGS([telepathy],[$DBUS_CFLAGS])
        fi
        dnl Power Management Inhibiter
        VLC_ADD_PLUGINS([inhibit])
        VLC_ADD_LIBS([inhibit],[$DBUS_LIBS])
        VLC_ADD_CFLAGS([inhibit],[$DBUS_CFLAGS])
        ],
    if ${PKG_CONFIG} --exists dbus-1
    then
      [AC_ARG_ENABLE(old-dbus,
        [  --enable-old-dbus       Deprecated D-Bus support (default disabled)])]

      if test "${enable_old_dbus}" != "yes"
      then
        echo ""
        echo "****** WARNING **** WARNING ***** WARNING *************"
        echo "*** Your D-Bus version is < 1.0.0"
        echo "*** You are using old and buggy software and so"
        echo "*** D-Bus support has been disabled."
        echo "*** Please upgrade D-Bus : http://dbus.freedesktop.org"
        echo "*** Alternatively you can force the use of your old and"
        echo "*** buggy D-Bus version by specifying --enable-old-dbus"
        echo "*******************************************************"
        echo ""
      else
        dnl not too old dbus
        [ PKG_CHECK_MODULES(DBUS, dbus-1 >= 0.92,
          [ AC_DEFINE( HAVE_DBUS_2, 1, [Define if you have the D-BUS library API >= 0.92] )
            AC_DEFINE( HAVE_DBUS_1, 1, [Define if you have the D-BUS library API >= 0.30] )
            AC_DEFINE( HAVE_DBUS, 1, [Define if you have the D-BUS library] )
            VLC_ADD_LIBS([screensaver],[$DBUS_LIBS])
            VLC_ADD_CFLAGS([screensaver],[$DBUS_CFLAGS])],
          dnl older dbus
          [ PKG_CHECK_MODULES(DBUS, dbus-1 >= 0.30,
            [ AC_DEFINE( HAVE_DBUS_1, 1, [Define if you have the D-BUS library API >= 0.30 ] )
              AC_DEFINE( HAVE_DBUS, 1, [Define if you have the D-BUS library] )
              VLC_ADD_LIBS([screensaver],[$DBUS_LIBS])
              VLC_ADD_CFLAGS([screensaver],[$DBUS_CFLAGS])],
            dnl much older dbus
            [ PKG_CHECK_MODULES( DBUS, dbus-1, 
               [AC_DEFINE(HAVE_DBUS, 1, [Define if you have the D-BUS library])
                VLC_ADD_LIBS([screensaver],[$DBUS_LIBS])
                VLC_ADD_CFLAGS([screensaver],[$DBUS_CFLAGS])],
               [AC_MSG_WARN(DBUS library not found)])
            ]
          )]
        )]
      fi
    fi
  )
fi
;;
esac

dnl Build the gtk_main plugins?
NEED_GTK_MAIN=no
NEED_GNOME_MAIN=no
NEED_GTK2_MAIN=no
NEED_GNOME2_MAIN=no

dnl build the qte plugin ?
NEED_QTE_MAIN=no

dnl Check for ntohl, etc.
AC_CACHE_CHECK([for ntohl in sys/param.h],
    [ac_cv_c_ntohl_sys_param_h],
    [CFLAGS="${CFLAGS_save} -Wall -Werror"
     AC_TRY_COMPILE([#include <sys/param.h>],
        [int meuh; ntohl(meuh);],
        ac_cv_c_ntohl_sys_param_h=yes, ac_cv_c_ntohl_sys_param_h=no)])
if test "${ac_cv_c_ntohl_sys_param_h}" != "no"; then
    AC_DEFINE(NTOHL_IN_SYS_PARAM_H, 1, Define if <sys/param.h> defines ntohl.)
fi
CFLAGS="${CFLAGS_save}"

RDC_PROG_CC_WFLAGS([all extra sign-compare undef pointer-arith bad-function-cast cast-align write-strings missing-prototypes volatile-register-var])
RDC_PROG_CC_FLAGS_IFELSE([-pipe])

dnl This helps detect broken plugins with undefined symbols
dnl Better fail loudly at build-time than silenty at run-time.
dnl Unfortunately, this sucks on FreeBSD (won't even link with -lc)
if test `uname -s` != FreeBSD; then
  RDC_PROG_LINK_FLAGS_IFELSE([-Wl,-z,defs], [
    VLC_ADD_LDFLAGS([plugin],[-Wl,-z,defs])
  ])
fi

dnl Check for various optimization flags
AC_CACHE_CHECK([if \$CC accepts -Os],
    [ac_cv_c_os],
    [CFLAGS="${CFLAGS_save} -Os"
     AC_TRY_COMPILE([],,ac_cv_c_os=yes, ac_cv_c_os=no)])
if test "${ac_cv_c_os}" != "no"; then
    CFLAGS_OPTIM_SIZE="${CFLAGS_OPTIM_SIZE} -Os"
else 
    AC_CACHE_CHECK([if \$CC accepts -O],
        [ac_cv_c_o],
        [CFLAGS="${CFLAGS_save} -O"
         AC_TRY_COMPILE([],,ac_cv_c_o=yes, ac_cv_c_o=no)])
    if test "${ac_cv_c_o}" != "no"; then
        if test "${ac_cv_c_o3}" = "no"; then
            CFLAGS_OPTIM_SIZE="${CFLAGS_OPTIM_SIZE} -O"
        fi
    fi
fi

AC_CACHE_CHECK([if \$CC accepts -O3],
    [ac_cv_c_o3],
    [CFLAGS="${CFLAGS_save} -O3"
     AC_TRY_COMPILE([],,ac_cv_c_o3=yes, ac_cv_c_o3=no)])
if test "${ac_cv_c_o3}" != "no" -a "x${enable_debug}" = "xno"; then
    CFLAGS_OPTIM_SPEED="${CFLAGS_OPTIM_SPEED} -O3"
else
    AC_CACHE_CHECK([if \$CC accepts -O2],
        [ac_cv_c_o2],
        [CFLAGS="${CFLAGS_save} -O2"
         AC_TRY_COMPILE([],,ac_cv_c_o2=yes, ac_cv_c_o2=no)])
    if test "${ac_cv_c_o2}" != "no"; then
        CFLAGS_OPTIM_SPEED="${CFLAGS_OPTIM_SPEED} -O2"
    else
        AC_CACHE_CHECK([if \$CC accepts -O],
            [ac_cv_c_o],
            [CFLAGS="${CFLAGS_save} -O"
             AC_TRY_COMPILE([],,ac_cv_c_o=yes, ac_cv_c_o=no)])
        if test "${ac_cv_c_o}" != "no"; then
            CFLAGS_OPTIM_SPEED="${CFLAGS_OPTIM_SPEED} -O"
        fi
    fi
fi

AC_CACHE_CHECK([if \$CC accepts -O0],
    [ac_cv_c_o0],
    [CFLAGS="${CFLAGS_save} -O0"
     AC_TRY_COMPILE([],,ac_cv_c_o0=yes, ac_cv_c_o0=no)])
if test "${ac_cv_c_o0}" != "no"; then
    CFLAGS_NOOPTIM="${CFLAGS_NOOPTIM} -O0"
fi

dnl Check for -ffast-math
AC_CACHE_CHECK([if \$CC accepts -ffast-math],
    [ac_cv_c_fast_math],
    [CFLAGS="${CFLAGS_save} -ffast-math"
     AC_TRY_COMPILE([],,ac_cv_c_fast_math=yes, ac_cv_c_fast_math=no)])
if test "${ac_cv_c_fast_math}" != "no"; then
    CFLAGS_OPTIM_SPEED="${CFLAGS_OPTIM_SPEED} -ffast-math"
fi

dnl Check for -funroll-loops
dnl Disabled on BeOS because BeOS' gcc is buggy and may crash with it
if test "${SYS}" != "beos"
then
  AC_CACHE_CHECK([if \$CC accepts -funroll-loops],
      [ac_cv_c_unroll_loops],
      [CFLAGS="${CFLAGS_save} -funroll-loops"
       AC_TRY_COMPILE([],,ac_cv_c_unroll_loops=yes, ac_cv_c_unroll_loops=no)])
  if test "${ac_cv_c_unroll_loops}" != "no"; then
      CFLAGS_OPTIM_SPEED="${CFLAGS_OPTIM_SPEED} -funroll-loops"
  fi
fi

dnl Check for -fomit-frame-pointer
AC_CACHE_CHECK([if \$CC accepts -fomit-frame-pointer],
    [ac_cv_c_omit_frame_pointer],
    [CFLAGS="${CFLAGS_save} -fomit-frame-pointer"
     AC_TRY_COMPILE([],,ac_cv_c_omit_frame_pointer=yes, ac_cv_c_omit_frame_pointer=no)])
if test "${ac_cv_c_omit_frame_pointer}" != "no"; then
    CFLAGS_OPTIM_NODEBUG="${CFLAGS_OPTIM_NODEBUG} -fomit-frame-pointer"
fi

dnl Check for -fvisibility=hidden
AC_CACHE_CHECK([if \$CC accepts -fvisibility=hidden],
    [ac_cv_c_visibility_hidden],
    [CFLAGS="${CFLAGS_save} -fvisibility=hidden"
     AC_TRY_COMPILE([],,ac_cv_c_visibility_hidden=yes, ac_cv_c_visibility_hidden=no)])
if test "${ac_cv_c_visibility_hidden}" != "no"; then
    VLC_ADD_CFLAGS([libvlc plugin],[-fvisibility=hidden])
fi

AM_CONDITIONAL(HAVE_COMPILER_EXPORT, [test "$SYS" = "mingw32" -o "${ac_cv_c_visibility_hidden}" != "no"])

dnl Check for -fvisibility-inlines-hidden (breaks x86_64 linkers)
dnl AC_LANG_PUSH(C++)
dnl     AC_CACHE_CHECK([if \$CXX accepts -fvisibility-inlines-hidden],
dnl         [ac_cv_c_visibility_inlines_hidden],
dnl         [CXXFLAGS="${CXXFLAGS_save} -fvisibility-inlines-hidden"
dnl          AC_TRY_COMPILE([],,ac_cv_c_visibility_inlines_hidden=yes, ac_cv_c_visibility_inlines_hidden=no)])
dnl     if test "${ac_cv_c_visibility_inlines_hidden}" != "no"; then
dnl         VLC_ADD_CXXFLAGS([libvlc plugin],[-fvisibility-inlines-hidden])
dnl     fi
dnl     CXXFLAGS="${CXXFLAGS_save}"
dnl AC_LANG_POP(C++)

dnl Check for -mdynamic-no-pic
dnl AC_CACHE_CHECK([if \$CC accepts -mdynamic-no-pic],
dnl     [ac_cv_c_dynamic_no_pic],
dnl     [CFLAGS="${CFLAGS_save} -mdynamic-no-pic"
dnl      AC_TRY_COMPILE([],,ac_cv_c_dynamic_no_pic=yes, ac_cv_c_dynamic_no_pic=no)])
dnl AS_IF([test "${ac_cv_c_dynamic_no_pic}" != "no"], [
dnl     VLC_ADD_CFLAGS([builtin],[-mdynamic-no-pic])
dnl     VLC_ADD_CFLAGS([libvlc],[-mdynamic-no-pic])
dnl ])

dnl Check for Darwin plugin linking flags
AC_CACHE_CHECK([if \$CC accepts -bundle -undefined error],
    [ac_cv_ld_darwin],
    [CFLAGS="${CFLAGS_save} -bundle -undefined error"
     AC_TRY_COMPILE([],,ac_cv_ld_darwin=yes, ac_cv_ld_darwin=no)])
if test "${ac_cv_ld_darwin}" != "no"; then
    VLC_ADD_LDFLAGS([plugin],[-bundle -undefined error])
fi

dnl Check for standard plugin linking flags
dnl BeOS' gcc needs -nostart instead of -shared, even if -shared isn't harmful (just a warning)
dnl AS_IF([test "${SYS}" = "beos"], [
dnl   VLC_ADD_LDFLAGS([plugin mozilla],[-nostart])
dnl ], [
dnl   AC_CACHE_CHECK([if \$CC accepts -shared],
dnl         [ac_cv_ld_plugins],
dnl         [CFLAGS="${CFLAGS_save} -shared"
dnl        AC_TRY_COMPILE([],, ac_cv_ld_plugins=yes, ac_cv_ld_plugins=no)])
dnl   AS_IF([test "${ac_cv_ld_plugins}" != "no"], [
dnl     VLC_ADD_LDFLAGS([plugin mozilla],[-shared])
dnl   ])
dnl ])

dnl Checks for __attribute__(aligned()) directive
AC_CACHE_CHECK([__attribute__ ((aligned ())) support],
    [ac_cv_c_attribute_aligned],
    [ac_cv_c_attribute_aligned=0
        CFLAGS="${CFLAGS_save} -Werror"
    for ac_cv_c_attr_align_try in 2 4 8 16 32 64; do
        AC_TRY_COMPILE([],
        [static char c __attribute__ ((aligned(${ac_cv_c_attr_align_try}))) = 0; return c;],
        [ac_cv_c_attribute_aligned="${ac_cv_c_attr_align_try}"])
    done
        CFLAGS="${CFLAGS_save}"])
if test "${ac_cv_c_attribute_aligned}" != "0"; then
    AC_DEFINE_UNQUOTED([ATTRIBUTE_ALIGNED_MAX],
        [${ac_cv_c_attribute_aligned}],[Maximum supported data alignment])
fi

dnl Checks for __attribute__(format()) directive
AC_CACHE_CHECK([__attribute__ ((format ())) support with function pointers],
    [ac_cv_c_attribute_format],
    [ac_cv_c_attribute_format=no
        CFLAGS="${CFLAGS_save} -Werror"
        AC_TRY_COMPILE([],
        [static void (*pf_printf)(const char *, ...) __attribute__ ((format(printf,1,2)));(void)pf_printf;],
        [ac_cv_c_attribute_format=yes])
        CFLAGS="${CFLAGS_save}"])
if test "${ac_cv_c_attribute_format}" != "no"; then
    AC_DEFINE(HAVE_ATTRIBUTE_FORMAT, 1, Support for __attribute__((format())) with function pointers)
fi

dnl Check for __attribute__((packed))
AC_CACHE_CHECK([for __attribute__((packed))],
  [ac_cv_c_attribute_packed],
  [ac_cv_c_attribute_packed=no
   AC_TRY_COMPILE(, [struct __attribute__((__packed__)) foo { int a; } b;],
                    [ac_cv_c_attribute_packed=yes])])
if test "${ac_cv_c_attribute_packed}" != "no"; then
  AC_DEFINE(HAVE_ATTRIBUTE_PACKED, 1, Support for __attribute__((packed)) for structs)
fi

dnl Check for __attribute__((visibility()))
AC_CACHE_CHECK([for __attribute__((visibility()))],
  [ac_cv_c_attribute_visibility],
  [ac_cv_c_attribute_visibility=no
   AC_TRY_COMPILE(, [extern __attribute__((visibility("default"))) int foo(int);],
                    [ac_cv_c_attribute_visibility=yes])])
if test "${ac_cv_c_attribute_visibility}" != "no"; then
  AC_DEFINE(HAVE_ATTRIBUTE_VISIBILITY, 1, Support for __attribute__((visibility())) for exporting symbols)
fi

dnl
dnl  Check the CPU
dnl
case "${host_cpu}" in
  "")
    ARCH=unknown
    ;;
  *)
    ARCH="${host_cpu}"
    ;;
esac

dnl
dnl  Enable profiling
dnl
AC_ARG_ENABLE(gprof,
[  --enable-gprof          gprof profiling (default disabled)])
AC_ARG_ENABLE(cprof,
[  --enable-cprof          cprof profiling (default disabled)])
test "${enable_gprof}" != "yes" && enable_gprof="no"
test "${enable_cprof}" != "yes" && enable_cprof="no"

dnl
dnl  default modules
dnl
VLC_ADD_PLUGINS([dummy logger memcpy])
VLC_ADD_PLUGINS([mpgv mpga m4v m4a h264 vc1 demux_cdg cdg ps pva avi asf mp4 rawdv rawvid nsv real aiff mjpeg demuxdump flacsys tta])
VLC_ADD_PLUGINS([cvdsub svcdsub spudec subsdec subsusf t140 dvbsub cc mpeg_audio lpcm a52 dts cinepak flac])
VLC_ADD_PLUGINS([deinterlace invert adjust transform wave ripple psychedelic gradient motionblur rv32 rotate noise grain extract sharpen seamcarving])
VLC_ADD_PLUGINS([converter_fixed mono])
VLC_ADD_PLUGINS([trivial_resampler ugly_resampler])
VLC_ADD_PLUGINS([trivial_channel_mixer trivial_mixer])
VLC_ADD_PLUGINS([playlist export nsc xtag])
VLC_ADD_PLUGINS([i420_rgb grey_yuv rawvideo blend scale image logo magnify puzzle colorthres])
VLC_ADD_PLUGINS([wav araw subtitle vobsub adpcm a52sys dtssys au ty voc xa nuv smf])
VLC_ADD_PLUGINS([access_directory access_file access_udp access_tcp])
VLC_ADD_PLUGINS([access_http access_mms access_ftp])
VLC_ADD_PLUGINS([access_filter_bandwidth])
VLC_ADD_PLUGINS([packetizer_mpegvideo packetizer_h264])
VLC_ADD_PLUGINS([packetizer_mpeg4video packetizer_mpeg4audio])
VLC_ADD_PLUGINS([packetizer_vc1])

ALIASES="${ALIASES} cvlc"

dnl
dnl default modules that need g++
dnl
AC_LANG_PUSH(C++)
VLC_ADD_PLUGINS([spatializer])
AC_LANG_POP(C++)

if test "${SYS}" != "mingwce"; then
  VLC_ADD_PLUGINS([access_fake access_filter_timeshift access_filter_record access_filter_dump])
  VLC_ADD_PLUGINS([gestures rc telnet hotkeys showintf marq podcast shout sap fake folder])
  VLC_ADD_PLUGINS([rss mosaic wall motiondetect clone crop erase bluescreen alphamask gaussianblur])
  VLC_ADD_PLUGINS([i420_yuy2 i422_yuy2 i420_ymga i422_i420 yuy2_i422 yuy2_i420 chroma_chain])
  VLC_ADD_PLUGINS([aout_file linear_resampler bandlimited_resampler])
  VLC_ADD_PLUGINS([float32_mixer spdif_mixer simple_channel_mixer])
  VLC_ADD_PLUGINS([dolby_surround_decoder headphone_channel_mixer normvol equalizer param_eq])
  VLC_ADD_PLUGINS([converter_float a52tospdif dtstospdif audio_format])
  ALIASES="${ALIASES} rvlc"
fi

dnl
dnl Some plugins aren't useful on some platforms
dnl
if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
    VLC_ADD_PLUGINS([screensaver motion])
elif test "${SYS}" != "mingwce"; then
    VLC_ADD_PLUGINS([ntservice access_smb dmo msn])
    VLC_ADD_LIBS([dmo],[-lole32 -luuid])
else
    VLC_ADD_PLUGINS([win32text])
fi
if test "${SYS}" = "darwin"; then
    VLC_ADD_PLUGINS([quartztext])
    VLC_ADD_LDFLAGS([quartztext],[-Wl,-framework,Carbon,-framework,ApplicationServices])
fi

dnl
dnl  Accelerated modules
dnl
MMX_MODULES="memcpymmx i420_rgb_mmx i420_yuy2_mmx i422_yuy2_mmx i420_ymga_mmx"
#MMX_MODULES="${MMX_MODULES} idctmmx motionmmx"
MMXEXT_MODULES="memcpymmxext"
#MMXEXT_MODULES="${MMXEXT_MODULES} idctmmxext motionmmxext"
THREEDNOW_MODULES="memcpy3dn"
SSE_MODULES=""
SSE2_MODULES="i420_rgb_sse2 i420_yuy2_sse2 i422_yuy2_sse2"
ALTIVEC_MODULES="memcpyaltivec i420_yuy2_altivec"
#ALTIVEC_MODULES="${ALTIVEC_MODULES} idctaltivec motionaltivec"

dnl  Check for fully workin MMX intrinsics
dnl  We need support for -mmmx, we need <mmintrin.h>, and we also need a
dnl  working compiler (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=23963)
AC_CACHE_CHECK([if \$CC groks MMX intrinsics],
    [ac_cv_c_mmx_intrinsics],
    [CFLAGS="${CFLAGS_save} -O -mmmx"
     AC_TRY_COMPILE([#include <mmintrin.h>
                     #include <stdint.h>
                     uint64_t frobzor;],
                    [__m64 a, b, c;
                     a = b = c = (__m64)frobzor;
                     a = _mm_slli_pi16(a, 3);
                     a = _mm_adds_pi16(a, b);
                     c = _mm_srli_pi16(c, 8);
                     c = _mm_slli_pi16(c, 3);
                     b = _mm_adds_pi16(b, c);
                     a = _mm_unpacklo_pi8(a, b);
                     frobzor = (uint64_t)a;],
                    [ac_cv_c_mmx_intrinsics=yes],
                    [ac_cv_c_mmx_intrinsics=no])])
if test "${ac_cv_c_mmx_intrinsics}" != "no"; then
  AC_DEFINE(HAVE_MMX_INTRINSICS, 1, Define if MMX intrinsics are available.)
  VLC_ADD_CFLAGS([${MMX_MODULES}],[-mmmx])
fi

dnl  Check for fully workin SSE2 intrinsics
dnl  We need support for -mmmx, we need <emmintrin.h>, and we also need a
dnl  working compiler (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=23963)
AC_CACHE_CHECK([if \$CC groks SSE2 intrinsics],
    [ac_cv_c_sse2_intrinsics],
    [CFLAGS="${CFLAGS_save} -O -msse2"
     AC_TRY_COMPILE([#include <emmintrin.h>
                     #include <stdint.h>
                     uint64_t frobzor;],
                    [__m128i a, b, c;
                     a = b = c = _mm_set1_epi64((__m64)frobzor);
                     a = _mm_slli_epi16(a, 3);
                     a = _mm_adds_epi16(a, b);
                     c = _mm_srli_epi16(c, 8);
                     c = _mm_slli_epi16(c, 3);
                     b = _mm_adds_epi16(b, c);
                     a = _mm_unpacklo_epi8(a, b);
                     frobzor = (uint64_t)_mm_movepi64_pi64(a);],
                    [ac_cv_c_sse2_intrinsics=yes],
                    [ac_cv_c_sse2_intrinsics=no])])
if test "${ac_cv_c_sse2_intrinsics}" != "no"; then
  AC_DEFINE(HAVE_SSE2_INTRINSICS, 1, Define if SSE2 intrinsics are available.)
  VLC_ADD_CFLAGS([${SSE2_MODULES}],[-msse2])
fi

AC_CACHE_CHECK([if \$CC groks MMX inline assembly],
    [ac_cv_mmx_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("packuswb %%mm1,%%mm2"::"r"(p));],
                    ac_cv_mmx_inline=yes, ac_cv_mmx_inline=no)])
if test "${ac_cv_mmx_inline}" != "no"; then
  AC_DEFINE(CAN_COMPILE_MMX, 1, Define if \$CC groks MMX inline assembly.)
  ACCEL_MODULES="${ACCEL_MODULES} ${MMX_MODULES}"
fi

AC_CACHE_CHECK([if \$CC groks MMX EXT inline assembly],
    [ac_cv_mmxext_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("maskmovq %%mm1,%%mm2"::"r"(p));],
                    ac_cv_mmxext_inline=yes, ac_cv_mmxext_inline=no)])
if test "${ac_cv_mmxext_inline}" != "no"; then
  AC_DEFINE(CAN_COMPILE_MMXEXT, 1, Define if \$CC groks MMX EXT inline assembly.)
  ACCEL_MODULES="${ACCEL_MODULES} ${MMXEXT_MODULES}"
fi

AC_CACHE_CHECK([if \$CC groks 3D Now! inline assembly],
    [ac_cv_3dnow_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("pfadd %%mm1,%%mm2"::"r"(p));],
                    ac_cv_3dnow_inline=yes, ac_cv_3dnow_inline=no)])
if test "${ac_cv_3dnow_inline}" != "no"; then
  AC_DEFINE(CAN_COMPILE_3DNOW, 1, Define if \$CC groks 3D Now! inline assembly.)
  ACCEL_MODULES="${ACCEL_MODULES} ${THREEDNOW_MODULES}"
fi

AC_CACHE_CHECK([if \$CC groks SSE inline assembly],
    [ac_cv_sse_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("xorps %%xmm1,%%xmm2"::"r"(p));],
                    ac_cv_sse_inline=yes, ac_cv_sse_inline=no)])
if test "${ac_cv_sse_inline}" != "no" -a "${SYS}" != "solaris"; then
  AC_DEFINE(CAN_COMPILE_SSE, 1, Define if \$CC groks SSE inline assembly.)
  ACCEL_MODULES="${ACCEL_MODULES} ${SSE_MODULES}"
fi

AC_CACHE_CHECK([if \$CC groks SSE2 inline assembly],
    [ac_cv_sse2_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("punpckhqdq %%xmm1,%%xmm2"::"r"(p));],
                    ac_cv_sse2_inline=yes, ac_cv_sse2_inline=no)])
if test "${ac_cv_sse2_inline}" != "no" -a "${SYS}" != "solaris"; then
  AC_DEFINE(CAN_COMPILE_SSE2, 1, Define if \$CC groks SSE2 inline assembly.)
  ACCEL_MODULES="${ACCEL_MODULES} ${SSE2_MODULES}"
fi

if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
AC_CACHE_CHECK([if \$CC groks AltiVec inline assembly],
    [ac_cv_altivec_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[asm volatile("vperm 0,1,2,3");],
         ac_cv_altivec_inline=yes,
         [CFLAGS="${CFLAGS_save} -Wa,-m7400"
          AC_TRY_COMPILE(,[asm volatile("vperm 0,1,2,3");],
            [ac_cv_altivec_inline="-Wa,-m7400"],
            ac_cv_altivec_inline=no)
         ])])
if test "${ac_cv_altivec_inline}" != "no"; then
  AC_DEFINE(CAN_COMPILE_ALTIVEC, 1, Define if \$CC groks AltiVec inline assembly.)
  if test "${ac_cv_altivec_inline}" != "yes"; then
    VLC_ADD_CFLAGS([idctaltivec],[${ac_cv_altivec_inline}])
    VLC_ADD_CFLAGS([motionaltivec],[${ac_cv_altivec_inline}])
    VLC_ADD_CFLAGS([memcpyaltivec],[${ac_cv_altivec_inline}])
    VLC_ADD_CFLAGS([i420_yuy2_altivec],[${ac_cv_altivec_inline}])
    VLC_ADD_CFLAGS([libvlc],[${ac_cv_altivec_inline}])
  fi
  ACCEL_MODULES="${ACCEL_MODULES} ${ALTIVEC_MODULES}"
fi

dnl The AltiVec C extensions
dnl
dnl There are several possible cases:
dnl - OS X PPC, gcc 4.x: use -mpim-altivec -force_cpusubtype_ALL, don't
dnl                      need <altivec.h>
dnl - OS X PPC, gcc 3.x: need -faltivec, don't need <altivec.h>
dnl - Linux PPC, gcc 3.4, 4.x: need <altivec.h> which requires -maltivec
dnl - Linux PPC, gcc 3.3: need <altivec.h> and -maltivec -mabi=altivec
dnl - Linux PPC, gcc 3.x: need <altivec.h> and -fvec
dnl - Others: test should fail
AC_CACHE_CHECK([if \$CC groks AltiVec C extensions],
  [ac_cv_c_altivec],
  [# OS X/PPC test (gcc 4.x)
   CFLAGS="${CFLAGS_save} -mpim-altivec -force_cpusubtype_ALL"
   AC_TRY_COMPILE([vector unsigned char foo;],
     [vec_ld(0, (unsigned char *)0);],
     [ac_cv_c_altivec="-mpim-altivec -force_cpusubtype_ALL"],
     [# OS X/PPC test (gcc 3.x)
      CFLAGS="${CFLAGS_save} -faltivec"
      AC_TRY_COMPILE([vector unsigned char foo;],
        [vec_ld(1 * sizeof(vector float), (unsigned char *)0);],
        [ac_cv_c_altivec="-faltivec"],
        dnl Below this are the Linux tests
        [# Linux/PPC test (gcc 4.x)
         CFLAGS="${CFLAGS_save} -maltivec"
         AC_TRY_COMPILE([#include <altivec.h>],
           [vec_ld(0, (unsigned char *)0);],
           [ac_cv_c_altivec="-maltivec"],
           [# Linux/PPC test (gcc 3.3)
            CFLAGS="${CFLAGS_save} -maltivec -mabi=altivec"
            AC_TRY_COMPILE([#include <altivec.h>],
              [vec_ld(0, (unsigned char *)0);],
              [ac_cv_c_altivec=""
               ac_cv_c_altivec_abi="-maltivec -mabi=altivec"],
              [# Linux/PPC test (gcc 3.3)
               CFLAGS="${CFLAGS_save} -fvec"
               AC_TRY_COMPILE([#include <altivec.h>],
                 [vec_ld(0, (unsigned char *)0);],
                 [ac_cv_c_altivec="-fvec"],
                 [ac_cv_c_altivec=no])
              ])
           ])
        ])
     ])
   CFLAGS="${CFLAGS_save}"
  ])

if test "${ac_cv_c_altivec}" != "no"; then
  CPPFLAGS="${CPPFLAGS_save} ${ac_cv_c_altivec}"
fi

AS_IF( test "${enable_altivec}" != "no",
[
  AC_CHECK_HEADERS(altivec.h)
  CPPFLAGS="${CPPFLAGS_save}"
  
  if test "${ac_cv_c_altivec}" != "no"; then
    AC_DEFINE(CAN_COMPILE_C_ALTIVEC, 1, Define if your compiler groks C AltiVec extensions.)
    VLC_ADD_CFLAGS([libvlc],[${ac_cv_c_altivec}])
    VLC_ADD_CFLAGS([idctaltivec motionaltivec],[${ac_cv_c_altivec}])
    VLC_ADD_CFLAGS([i420_yuy2_altivec memcpyaltivec deinterlace],[${ac_cv_c_altivec} ${ac_cv_c_altivec_abi}])
    if test "${ac_cv_altivec_inline}" = "no"; then
      ACCEL_MODULES="${ACCEL_MODULES} ${ALTIVEC_MODULES}"
    fi
  fi
],[
  CPPFLAGS="${CPPFLAGS_save}"
])

AC_CACHE_CHECK([if linker needs -framework vecLib],
    [ac_cv_ld_altivec],
    [LDFLAGS="${LDFLAGS_vlc} -Wl,-framework,vecLib"
     AC_TRY_LINK([],,ac_cv_ld_altivec=yes,ac_cv_ld_altivec=no)
     LDFLAGS="${LDFLAGS_save}"
    ])
if test "${ac_cv_ld_altivec}" != "no"; then
  VLC_ADD_LDFLAGS([libvlc idctaltivec motionaltivec memcpyaltivec],[-Wl,-framework,vecLib])
fi
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

AC_ARG_WITH(,[])
AC_ARG_WITH(,[Optimization options:])

dnl
dnl  Special arch tuning
dnl
AC_ARG_WITH(tuning,
[  --with-tuning=ARCH      enable special tuning for an architecture
                          (default Pentium 2 on IA-32 and G4 on PPC)])
if test -n "${with_tuning}"; then
    if test "${with_tuning}" != "no"; then
        CFLAGS_TUNING="-mtune=${with_tuning}"
    fi
else
    if test "${SYS}" = "darwin" -a "${host_cpu}" != "powerpc"; then
        CFLAGS_TUNING="-march=pentium-m -mtune=prescott"
    elif test "${host_cpu}" = "i686" -o "${host_cpu}" = "i586" -o "${host_cpu}" = "i486" -o "${host_cpu}" = "i386"; then
        CFLAGS_TUNING="-mtune=pentium2"
    elif test "${host_cpu}" = "x86_64"; then
        CFLAGS_TUNING="-mtune=athlon64"
    elif test "${host_cpu}" = "powerpc"; then
        CFLAGS_TUNING="-mtune=G4";
    fi
fi

dnl NOTE: this can't be cached cleanly
AS_IF([test "${CFLAGS_TUNING}"],
   [CFLAGS_save="${CFLAGS}"
    CFLAGS="${CFLAGS} ${CFLAGS_TUNING}"

    AC_MSG_CHECKING([whether $CC accepts ${CFLAGS_TUNING}])
    AC_COMPILE_IFELSE([ ],
                      [tuning="yes"],
                      [CFLAGS_TUNING=""; tuning="no"
                       AS_IF([test "${with_tuning}"],
                             [AC_MSG_ERROR([requested tuning not supported])])])

    AC_MSG_RESULT([$tuning])
    CFLAGS="${CFLAGS_save}"
])

dnl
dnl  x86 accelerations
dnl
AC_ARG_ENABLE(mmx,
[  --disable-mmx       disable MMX optimizations (default enabled on x86)],
[ if test "${enable_mmx}" = "yes"; then ARCH="${ARCH} mmx";
    VLC_ADD_PLUGINS([${ACCEL_MODULES}]) fi ],
[ if test "${host_cpu}" = "i686" -o "${host_cpu}" = "i586" -o "${host_cpu}" = "x86" -o "${host_cpu}" = "i386" -o "${host_cpu}" = "i486" -o "${host_cpu}" = "x86_64"; then
    ARCH="${ARCH} mmx"
    VLC_ADD_PLUGINS([${ACCEL_MODULES}]) fi ])

AC_ARG_ENABLE(sse,
[  --disable-sse       disable SSE (1 and 2) optimizations (default enabled on i686 and x86_64)],
[ if test "${enable_sse}" = "yes"; then ARCH="${ARCH} sse sse2";
    VLC_ADD_PLUGINS([${ACCEL_MODULES}]) fi ],
[ if test "${host_cpu}" = "i686" -o "${host_cpu}" = "x86_64"; then ARCH="${ARCH} sse sse2";
    VLC_ADD_PLUGINS([${ACCEL_MODULES}]) fi ])

dnl
dnl  Memory usage
dnl
AC_ARG_ENABLE(optimize-memory,
[  --enable-optimize-memory optimize memory usage over performance])
if test "${enable_optimize_memory}" = "yes"; then
  AC_DEFINE(OPTIMIZE_MEMORY, 1, Define if you want to optimize memory usage over performance)
fi

dnl
dnl  Enable/disable optimizations
dnl
AC_ARG_ENABLE(optimizations,
[  --disable-optimizations disable compiler optimizations (default enabled)])
if test "${enable_optimizations}" != "no"; then
   if test "${enable_optimize_memory}" = "yes"; then
      enable_optimizations="size"
   else
      enable_optimizations="speed"
   fi
fi

dnl
dnl  AltiVec acceleration
dnl
AC_ARG_ENABLE(altivec,
[  --disable-altivec       disable AltiVec optimizations (default enabled on PPC)],
[ if test "${enable_altivec}" = "yes"; then ARCH="${ARCH} altivec";
    VLC_ADD_PLUGINS([${ACCEL_MODULES}]) fi ],
[ if test "${host_cpu}" = "powerpc"; then ARCH="${ARCH} altivec";
    VLC_ADD_PLUGINS([${ACCEL_MODULES}]) fi ])

dnl
dnl  Debugging mode
dnl
AC_ARG_ENABLE(debug,
[  --enable-debug          debug mode (default disabled)])
test "${enable_debug}" != "yes" && enable_debug="no"
AH_TEMPLATE(NDEBUG,
            [Define to 1 if debug code should NOT be compiled])
AS_IF([test "x${enable_debug}" = "xno"], [AC_DEFINE(NDEBUG)])

dnl
dnl  Test coverage
dnl
AC_ARG_ENABLE(coverage,
	[  --enable-coverage       build for test coverage (default disabled)],,
	[enable_coverage="no"])
AS_IF([test "${enable_coverage}" != "no"], [
	CFLAGS="-fprofile-arcs -ftest-coverage ${CFLAGS}"
	CXXFLAGS="-fprofile-arcs -ftest-coverage ${CXXFLAGS}"
	LDFLAGS="-lgcov ${LDFLAGS}"
	dnl ugly...
	CFLAGS_save="${CFLAGS}"
	CXXFLAGS_save="${CXXFLAGS}"
	LDFLAGS_save="${LDFLAGS}"
])

dnl
dnl  Enable release-specific flags
dnl
AC_ARG_ENABLE(release,
[  --enable-release        activate extra optimizations (default disabled)])
test "${enable_release}" != "yes" && enable_release="no"

dnl
dnl Stream output
dnl
AC_ARG_ENABLE(sout,
  [  --enable-sout           Stream output modules (default enabled)])
if test "${enable_sout}" != "no"
then
  VLC_ADD_PLUGINS([access_output_dummy access_output_udp access_output_file access_output_http])
  VLC_ADD_PLUGINS([mux_ps mux_avi mux_mp4 mux_asf mux_dummy mux_wav mux_mpjpeg])
  VLC_ADD_PLUGINS([packetizer_copy])

  VLC_ADD_PLUGINS([stream_out_dummy stream_out_standard stream_out_es stream_out_rtp stream_out_description vod_rtsp])
  VLC_ADD_PLUGINS([stream_out_duplicate stream_out_display stream_out_transcode stream_out_bridge stream_out_mosaic_bridge stream_out_autodel])
  VLC_ADD_PLUGINS([stream_out_gather])
#  VLC_ADD_PLUGINS([stream_out_transrate])
#  VLC_ADD_PLUGINS([rtcp])
  VLC_ADD_PLUGINS([profile_parser])

  AC_DEFINE(ENABLE_SOUT, 1, Define if you want the stream output support)
fi

AC_ARG_ENABLE(switcher,
  [  --enable-switcher       Stream-out switcher plugin (default disabled)])

dnl Check for libshout
AC_ARG_ENABLE(shout,
  [  --enable-shout          libshout output plugin (default disabled)])
if test "${enable_shout}" = "yes"; then
  PKG_CHECK_MODULES(SHOUT, shout >= 2.1,
    [AC_DEFINE(HAVE_SHOUT, 1, [Define if you have the libshout library])
     VLC_ADD_PLUGINS([access_output_shout])
     VLC_ADD_LIBS([access_output_shout],[$SHOUT_LIBS])
     VLC_ADD_CFLAGS([access_output_shout],[$SHOUT_CFLAGS])],
    [AC_MSG_WARN(libshout library not found)])
fi

dnl Lua modules
AC_ARG_ENABLE(lua,
  [  --enable-lua            lua playlist and metafetcher plugins (default enabled)])
if test "${enable_lua}" != "no"
then
  PKG_CHECK_MODULES(LUA, lua5.1,
    [ have_lua=yes ],
    [
    AC_MSG_WARN([lua5.1 not found, trying lua >= 5.1 instead])
    PKG_CHECK_MODULES(LUA, lua >= 5.1,
      [ have_lua=yes ],
      [ 
        have_lua=yes
        AC_CHECK_HEADERS([lua.h lauxlib.h lualib.h], 
	  [],  
          [ have_lua=no ] )
	AC_CHECK_LIB(  lua5.1 , luaL_newstate, 
          [LUA_LIBS="-llua5.1"],
	  AC_CHECK_LIB( lua51 , luaL_newstate, 
            [LUA_LIBS="-llua51"],
		  AC_CHECK_LIB( lua , luaL_newstate, 
		    [LUA_LIBS="-llua"],
	      [ have_lua=no
                AC_MSG_WARN([lua >= 5.1 not found!])
              ])
	    )
	  )
      ])
    ])
  if test "x${have_lua}" = "xyes" ;  then
     AC_DEFINE(HAVE_LUA, 1, [Define if you have the lua library])
     VLC_ADD_PLUGINS([lua])
     VLC_ADD_LIBS([lua],[$LUA_LIBS])
     VLC_ADD_CFLAGS([lua],[$LUA_CFLAGS])
  fi
fi

dnl
dnl HTTP daemon
dnl
AC_ARG_ENABLE(httpd,
  [  --enable-httpd          HTTP daemon (default enabled)])
if test "${enable_httpd}" != "no"
then
  VLC_ADD_PLUGINS([http])
  AC_DEFINE(ENABLE_HTTPD, 1, Define if you want the HTTP dameon support)
fi

dnl
dnl VideoLAN manager
dnl
AC_ARG_ENABLE(vlm,
  [  --enable-vlm            VideoLAN manager (default enabled)])
if test "${enable_vlm}" != "no"
then
  AC_DEFINE(ENABLE_VLM, 1, Define if you want the VideoLAN manager support)
fi

dnl
dnl Growl notification plugin
dnl
AC_ARG_ENABLE(growl,
  [  --enable-growl          growl notification plugin (default enabled)])
AS_IF([test "${enable_growl}" != "no"], [
    VLC_ADD_PLUGINS([growl_udp])
    AC_CHECK_HEADERS(Growl/GrowlDefines.h, [
      VLC_ADD_PLUGINS([growl])
      VLC_ADD_LDFLAGS([growl], [-Wl,-framework,Growl,-framework,AppKit])
    ])
  ]
)

dnl
dnl Libnotify notification plugin
dnl

AC_ARG_ENABLE(notify,
  [  --enable-notify         libnotify notification plugin (default enabled)])
AS_IF([test "${enable_notify}" != "no" -a "${SYS}" != "mingw32"], [
    PKG_CHECK_MODULES(NOTIFY, libnotify,
      [
        VLC_ADD_PLUGINS([notify])
        VLC_ADD_CFLAGS(notify, [$NOTIFY_CFLAGS])
        VLC_ADD_LIBS(notify, [$NOTIFY_LIBS])
      ],[
    AS_IF([test "${enable_notify}" = "yes"],[
        AC_MSG_WARN( libnotify not found) ])
      ])
  ])

dnl
dnl Audioscrobbler plugin
dnl

AC_ARG_ENABLE(audioscrobbler,
  [  --disable-audioscrobbler Last.fm submission plugin (default enabled)])
AS_IF([test "${enable_audioscrobbler}" != "no"], [
        VLC_ADD_PLUGINS([audioscrobbler])
  ])

dnl
dnl Musicbrainz plugin
dnl
AC_ARG_ENABLE(musicbrainz,
  [  --enable-musicbrainz    MusicBrainz support (default disabled) ])
  AS_IF([test "${enable_musicbrainz}" = "yes"],[
      PKG_CHECK_MODULES(MUSICBRAINZ, libmusicbrainz,
    [ VLC_ADD_PLUGINS([musicbrainz])
      VLC_ADD_LIBS([musicbrainz],[$MUSICBRAINZ_LIBS])
      VLC_ADD_CFLAGS([musicbrainz],[$MUSICBRAINZ_CFLAGS])  ],
       [AC_MSG_WARN(MusicBrainz library not found)])
   ])


dnl
dnl Taglibplugin
dnl
AC_ARG_ENABLE(taglib,
  [  --disable-taglib Taglib support (default enabled) ])
  AS_IF([test "${enable_taglib}" != "no"],[
    PKG_CHECK_MODULES(TAGLIB, taglib,
      [ AC_DEFINE( HAVE_TAGLIB, 1, [Define if you have taglib plugin built] )
        VLC_ADD_PLUGINS([taglib])
        VLC_ADD_LIBS([taglib],[$TAGLIB_LIBS -lz])
       VLC_ADD_CXXFLAGS([taglib],[$TAGLIB_CFLAGS]) ],
      [AC_MSG_WARN(TagLib library not found)])
  ])


dnl
dnl  Input plugins
dnl

AC_ARG_WITH(,[Input plugins:])

dnl  live555 input
dnl
if test "${enable_livedotcom}" 
then
  AC_MSG_WARN(--{en|dis}able-livedotcom is deprecated. Use --{en|dis}able-live555 instead.)
fi

if test "${enable_livedotcom}" = "no"
then
  enable_live555="no"
fi
AC_ARG_ENABLE(live555,
[  --enable-live555        live555 RTSP input plugin (default enabled)])
if test "${enable_live555}" != "no"; then
  AC_ARG_WITH(live555-tree,
    [    --with-live555-tree=PATH live.com tree for static linking])

  dnl
  dnl test for --with-live555-tree
  dnl
  if test "${with_livedotcom_tree}" 
  then
    AC_MSG_WARN(--with-livedotcom-tree is deprecated. Use --with-live555-tree instead.)
  with_live555_tree="${with_livedotcom_tree}"
  fi

  if test -z "${with_live555_tree}" -a "${CXX}" != ""; then
    AC_LANG_PUSH(C++)
    CPPFLAGS_save="${CPPFLAGS}"
    CPPFLAGS_live555="-I/usr/include/liveMedia -I/usr/include/groupsock -I/usr/include/BasicUsageEnvironment -I/usr/include/UsageEnvironment"
    if test "${SYS}" = "solaris"; then
      CPPFLAGS_live555="${CPPFLAGS_live555} -DSOLARIS"
    fi
    CPPFLAGS="${CPPFLAGS} ${CPPFLAGS_live555}"
    AC_CHECK_HEADERS(liveMedia.hh, [
      VLC_ADD_CXXFLAGS([live555], [${CPPFLAGS_live555}])
      AC_CHECK_LIB(liveMedia_pic, main, [
        # We have -lliveMedia_pic, build plugins
        VLC_ADD_PLUGINS([live555])
        VLC_ADD_LIBS([live555], [-lliveMedia_pic -lgroupsock_pic -lBasicUsageEnvironment_pic -lUsageEnvironment_pic])
      ], [
        AC_CHECK_LIB(liveMedia, main, [
          # We only have -lliveMedia, do builtins
          VLC_ADD_BUILTINS([live555])
          VLC_ADD_LIBS([live555], [-lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment])
        ])
      ])
      if test "${SYS}" = "mingw32"; then
        # add ws2_32 for closesocket, select, recv
        VLC_ADD_LIBS([live555],[-lws2_32])
      fi
    ])
    CPPFLAGS="${CPPFLAGS_save}"
    AC_LANG_POP(C++)
  else
    AC_MSG_CHECKING(for liveMedia/libliveMedia.a in ${with_live555_tree})
    real_live555_tree="`cd ${with_live555_tree} 2>/dev/null && pwd`"
    if test -z "${real_live555_tree}"; then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_live555_tree}])
    fi
    if test -f "${real_live555_tree}/liveMedia/libliveMedia.a"; then
      AC_MSG_RESULT(${real_live555_tree}/liveMedia/libliveMedia.a)

      VLC_ADD_BUILTINS([live555])

      if test "${SYS}" = "mingw32"; then
        # add ws2_32 for closesocket, select, recv
        VLC_ADD_LIBS([live555],[-lws2_32])
      fi

      VLC_ADD_LIBS([live555],[-L${real_live555_tree}/liveMedia -lliveMedia])
      VLC_ADD_LIBS([live555],[-L${real_live555_tree}/BasicUsageEnvironment -lBasicUsageEnvironment])
      VLC_ADD_LIBS([live555],[-L${real_live555_tree}/groupsock -lgroupsock])
      VLC_ADD_LIBS([live555],[-L${real_live555_tree}/UsageEnvironment -lUsageEnvironment])

      VLC_ADD_CXXFLAGS([live555],[-I${real_live555_tree}/BasicUsageEnvironment/include])
      VLC_ADD_CXXFLAGS([live555],[-I${real_live555_tree}/groupsock/include])
      VLC_ADD_CXXFLAGS([live555],[-I${real_live555_tree}/liveMedia/include])
      VLC_ADD_CXXFLAGS([live555],[-I${real_live555_tree}/UsageEnvironment/include ])
      if test "${SYS}" = "solaris"; then
        VLC_ADD_CXXFLAGS([live555],[-DSOLARIS])
      fi
    else
      dnl  The given live555 wasn't built
        AC_MSG_RESULT(no)
      if test "${enable_live555}" = "yes"; then
        AC_MSG_ERROR([cannot find ${real_live555_tree}/liveMedia/libliveMedia.a, make sure you compiled live555 in ${with_live555_tree}])
      fi	
    fi
  fi
fi

dnl
dnl  special access module for dc1394 input
dnl
AC_ARG_ENABLE(dc1394,
  [  --enable-dc1394         dc1394 access module (default disabled)])
if test "${enable_dc1394}" = "yes"
then
  AC_CHECK_HEADERS(libraw1394/raw1394.h, [
    AC_CHECK_LIB( raw1394, raw1394_get_nodecount, [
        AC_CHECK_HEADERS(libdc1394/dc1394_control.h , [
dnl         AC_CHECK_LIB( dc1394_control, dc1394_setup_capture, [
              VLC_ADD_PLUGINS([dc1394])
              VLC_ADD_LIBS([dc1394],[-ldc1394_control -lraw1394])
dnl         ],
dnl         [
dnl           AC_MSG_ERROR([libdc1394 is mandatory for the dc1394 input module. try --disable-dc1394 or install this library])
dnl         ])
      ],
      [
        AC_MSG_ERROR([libdc1394 is mandatory for the dc1394 input module. try --disable-dc1394 or install this library])
      ])
    ],
    [
      AC_MSG_ERROR([libraw1394 is mandatory for the dc1394 input module. try --disable-dc1394 or install this library])
    ])
  ],
  [
    AC_MSG_ERROR([libraw1394 is mandatory for the dc1394 input module. try --disable-dc1394 or install this library])
  ])
fi

dnl
dnl dv module: digital video module check for libraw1394
dnl
AC_ARG_ENABLE(dv,
[  --enable-dv             dv input module (default disabled)])
if test "${enable_dv}" = "yes"
then
  AC_ARG_WITH(dv-raw1394,
  [    --with-dv-raw1394=PATH   libraw1394 headers and libraries])
  AC_ARG_WITH(dv-raw1394-tree,
  [    --with-dv-raw1394=PATH   libraw1394 tree for static linking])

  if test -z "${with_dv_raw1394}" -a "${with_dv_raw1394}" != ""
  then
    AC_MSG_CHECKING(for raw1394 headers in ${with_dv_raw1394})
    if test -f ${with_dv_raw1394}/include/libraw1394/raw1394.h
    then
      dnl  Use ${with_dv_raw1394}/include/libraw1394/raw1394.h
      AC_MSG_RESULT(yes)
      VLC_ADD_PLUGINS([access_dv])
      VLC_ADD_LIBS([access_dv],[-L${with_dv_raw1394}/lib -lraw1394 -lpthread])
      VLC_ADD_CPPFLAGS([access_dv],[-I${with_dv_raw1394}/include])
    else
      dnl  No libraw1394 could be found, sorry
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${with_dv_raw1394}/include/libraw1394/raw1394.h])
    fi
  else
    AC_CHECK_HEADERS(libraw1394/raw1394.h,
    [ VLC_ADD_PLUGINS([access_dv])
        VLC_ADD_LIBS([access_dv],[-lraw1394 -lavc1394])
    ],[
        if test -n "${enable_dv}"
        then
          AC_MSG_ERROR([cannot find libraw1394 headers])
        fi
    ])
  fi

  dnl Check for static linking of libraw1394
  if test -z "${with_dv_raw1394_tree}" -a "${with_dv_raw1394_tree}" != ""
  then
    AC_MSG_CHECKING(for libraw1394.a in ${with_dv_raw1394_tree})
    real_dv_raw1394_tree="`cd ${with_dv_raw1394_tree} 2>/dev/null && pwd`"
    if test -z "${real_dv_raw1394_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${real_dv_raw1394_tree}])
    fi
    if test -f "${real_dv_raw1394_tree}/src/.libs/libraw1394.a"
    then
      dnl  Use a custom libraw1394
      AC_MSG_RESULT(${real_dv_raw1394_tree}/src/.libs/libraw1394.a)
      VLC_ADD_BUILTINS([access_dv])
      VLC_ADD_LIBS([access_dv],[-L${real_dv_raw1394_tree}/src/.libs -lraw1394])
      VLC_ADD_CPPFLAGS([access_dv],[-I${real_dv_raw1394_tree}])
    else
      dnl  The given libraw1394 wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_dv_raw1394_tree}/src/.libs/libraw1394.a, make sure you compiled libraw1394 in ${with_dv_raw1394_tree}])
    fi
  fi

  dnl
  dnl Check for libavc1394
  dnl

  AC_ARG_WITH(dv-avc1394,
  [    --with-dv-avc1394=PATH   libavc1394 headers and libraries])
  AC_ARG_WITH(dv-avc1394-tree,
  [    --with-dv-avc1394=PATH   libavc1394 tree for static linking])

  if test -z "${with_dv_avc1394}" -a "${with_dv_avc1394}" != ""
  then
    AC_MSG_CHECKING(for avc1394 headers in ${with_dv_avc1394})
    if test -f ${with_dv_avc1394}/include/libavc1394/avc1394.h
    then
      dnl  Use ${with_dv_avc1394}/include/libavc1394/avc1394.h
      AC_MSG_RESULT(yes)
      VLC_ADD_LIBS([access_dv],[-L${with_dv_avc1394}/lib -lavc1394 -lrom1394 -lpthread])
      VLC_ADD_CPPFLAGS([access_dv],[-I${with_avc1394}/include])
    else
      dnl  No libavc1394 could be found, sorry
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${with_dv_avc1394}/include/libavc1394/avc1394.h])
    fi
  else
    AC_CHECK_HEADERS(libavc1394/avc1394.h,
    [ VLC_ADD_LIBS([access_dv],[-lavc1394 -lrom1394 -lpthread])
    ],[
        if test -n "${enable_dv}"
        then
          AC_MSG_ERROR([cannot find libavc1394 headers])
        fi
    ])
  fi

  dnl Check for static linking of libavc1394
  if test -z "${with_dv_avc1394_tree}" -a "${with_dv_avc1394_tree}" != ""
  then
    AC_MSG_CHECKING(for libavc1394.a in ${with_dv_avc1394_tree})
    real_dv_avc1394_tree="`cd ${with_dv_avc1394_tree} 2>/dev/null && pwd`"
    if test -z "${real_dv_avc1394_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${real_dv_avc1394_tree}])
    fi
    if test -f "${real_dv_avc1394_tree}/src/.libs/libavc1394.a"
    then
      dnl  Use a custom libavc1394
      AC_MSG_RESULT(${real_dv_avc1394_tree}/src/.libs/libavc1394.a)
      VLC_ADD_LIBS([access_dv],[-L${real_dv_avc1394_tree}/src/.libs -lavc1394 -lrom1394 -lpthread])
      VLC_ADD_CPPFLAGS([access_dv],[-I${real_dv_avc1394_tree}])
    else
      dnl  The given libavc1394 wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_dv_avc1394_tree}/src/.libs/libavc1394.a, make sure you compiled libavc1394 in ${with_dv_avc1394_tree}])
    fi
  fi
fi

dnl
dnl dvdread module: check for libdvdread
dnl
AC_ARG_ENABLE(dvdread,
[  --enable-dvdread        dvdread input module (default disabled)])
if test "${enable_dvdread}" != "no"
then
  AC_ARG_WITH(dvdread,
  [    --with-dvdread=PATH   libdvdread headers and libraries])
  AC_ARG_WITH(dvdread-tree,
  [    --with-dvdread-tree=PATH libdvdread tree for static linking])

  dnl Temporary hack (yeah, sure ;)
  if test "${SYS}" = "mingw32" || test "${SYS}" = "darwin" || test "${SYS}" = "beos" ; then
      VLC_ADD_LIBS([dvdread],[-ldvdcss])
  fi

  if test -z "${with_dvdread}"
  then
    if test -z "${with_dvdread_tree}"
    then
      AC_CHECK_HEADERS(dvdread/dvd_reader.h,
        [ VLC_ADD_PLUGINS([dvdread])
          VLC_ADD_LIBS([dvdread],[-ldvdread ${LIBS_dvdcss}])
        ],[
          if test -n "${enable_dvdread}"
          then
            AC_MSG_ERROR([cannot find libdvdread headers])
          fi
        ])
    else
      AC_MSG_CHECKING(for libdvdread.a in ${with_dvdread_tree})
      real_dvdread_tree="`cd ${with_dvdread_tree} 2>/dev/null && pwd`"
      if test -z "${real_dvdread_tree}"
      then
        dnl  The given directory can't be found
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot cd to ${with_dvdread_tree}])
      fi
      if test -f "${real_dvdread_tree}/dvdread/.libs/libdvdread.a"
      then
        dnl  Use a custom libdvdread
        AC_MSG_RESULT(${real_dvdread_tree}/dvdread/.libs/libdvdread.a)
        VLC_ADD_BUILTINS([dvdread])
        VLC_ADD_LIBS([dvdread],[-L${real_dvdread_tree}/dvdread/.libs -ldvdread ${LIBS_dvdcss}])
        VLC_ADD_CPPFLAGS([dvdread],[-I${real_dvdread_tree}])
      else
        dnl  The given libdvdread wasn't built
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot find ${real_dvdread_tree}/dvdread/.libs/libdvdread.a, make sure you compiled libdvdread in ${with_dvdread_tree}])
      fi
    fi
  else
    AC_MSG_CHECKING(for dvdread headers in ${with_dvdread})
    if test -f ${with_dvdread}/include/dvdread/dvd_reader.h
    then
      dnl  Use ${with_dvdread}/include/dvdread/dvd_reader.h
      AC_MSG_RESULT(yes)
      VLC_ADD_PLUGINS([dvdread])
      VLC_ADD_LIBS([dvdread],[-L${with_dvdread}/lib -ldvdread ${LIBS_dvdcss}])
      VLC_ADD_CPPFLAGS([dvdread],[-I${with_dvdread}/include])
    else
      dnl  No libdvdread could be found, sorry
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${with_dvdread}/include/dvdread/dvd_reader.h])
    fi
  fi
fi

dnl
dnl  libdvdnav plugin
dnl
AC_ARG_ENABLE(dvdnav,
  [  --enable-dvdnav         dvdnav input module (default enabled)])
if test "${enable_dvdnav}" != "no"
then
  dnl Same hack than dvdread
  if test "${SYS}" = "mingw32" || test "${SYS}" = "darwin" || test "${SYS}" = "beos" ; then
      VLC_ADD_LIBS([dvdnav],[-ldvdcss])
  fi

  DVDNAV_PATH="${PATH}"
  AC_ARG_WITH(dvdnav-config-path,
    [    --with-dvdnav-config-path=PATH dvdnav-config path (default search in \$PATH)],
    [ if test "${with_dvdnav_config_path}" != "no"
      then
        DVDNAV_PATH="${with_dvdnav_config_path}:${PATH}"
      fi ])
  AC_PATH_PROG(DVDNAV_CONFIG, dvdnav-config, no, ${DVDNAV_PATH})
  if test "${DVDNAV_CONFIG}" != "no"
  then
    VLC_ADD_PLUGINS([dvdnav])
    VLC_ADD_CFLAGS([dvdnav],[`${DVDNAV_CONFIG} --cflags`])
    VLC_ADD_LIBS([dvdnav],[`${DVDNAV_CONFIG} --libs`])
  fi
fi

dnl
dnl  Windows DirectShow access module
dnl
AC_ARG_ENABLE(dshow,
  [  --enable-dshow          Win32 DirectShow support (default enabled on Win32)])
if test "${enable_dshow}" != "no"
then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"
  then
    AC_LANG_PUSH(C++)
      AC_CHECK_HEADERS(dshow.h,
      [ VLC_ADD_PLUGINS([dshow])
        VLC_ADD_CXXFLAGS([dshow],[])
        VLC_ADD_LIBS([dshow],[-lole32 -loleaut32 -luuid]) ])
    AC_LANG_POP(C++)
  fi
fi

dnl
dnl  Windows DirectShow BDA access module
dnl
AC_ARG_ENABLE(bda,
  [  --enable-bda            Win32 DirectShow BDA support (default enabled on Win32)])
if test "${enable_bda}" != "no"
then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"
  then
      AC_CHECK_HEADERS(dshow.h,
      [ VLC_ADD_PLUGINS([bda])
        VLC_ADD_CXXFLAGS([bda],[])
        VLC_ADD_LIBS([bda],[-lstrmiids -lole32 -loleaut32 -luuid]) ])
  fi
fi


dnl
dnl  OpenCV wrapper and example filters
dnl
AC_ARG_ENABLE(opencv,
  [  --enable-opencv         OpenCV (computer vision) filter (default disabled)])
if test "${enable_opencv}" = "yes" -a "${CXX}" != "";
then
  AC_ARG_WITH(opencv-tree,
  [    --with-opencv-tree=PATH opencv tree for linking])
  if test -n "${with_opencv_tree}"
  then
    if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"
    then
	  AC_MSG_CHECKING(for opencv in ${with_opencv_tree})
	  if test -f ${with_opencv_tree}/cv/include/cv.h -a -f ${with_opencv_tree}/cxcore/include/cxcore.h \
	    -a -f ${with_opencv_tree}/cvaux/include/cvaux.h -a -f ${with_opencv_tree}/otherlibs/highgui/highgui.h
      then
        AC_MSG_RESULT(yes)
        VLC_ADD_PLUGINS([opencv_wrapper])
        VLC_ADD_LIBS([opencv_wrapper],[-L${with_opencv_tree}/lib -lcv -lcxcore -lcvaux -lhighgui])
        VLC_ADD_CFLAGS([opencv_wrapper],[-I${with_opencv_tree}/cv/include -I${with_opencv_tree}/cxcore/include -I${with_opencv_tree}/cvaux/include -I${with_opencv_tree}/otherlibs/highgui])
        AC_LANG_PUSH(C++)
        VLC_ADD_PLUGINS([opencv_example])
        VLC_ADD_LIBS([opencv_example],[-L${with_opencv_tree}/lib -lcv -lcxcore -lcvaux -lhighgui])
        VLC_ADD_CXXFLAGS([opencv_example],[-I${with_opencv_tree}/cv/include -I${with_opencv_tree}/cxcore/include -I${with_opencv_tree}/cvaux/include -I${with_opencv_tree}/otherlibs/highgui])
        AC_LANG_POP(C++)        
	  else
        dnl  No opencv could be found, sorry
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot find opencv in ${with_opencv_tree}])
	  fi
    else
	  AC_MSG_WARN([--enable-opencv currently only works on windows])
    fi
  fi  
fi


dnl
dnl  libsmbclient plugin
dnl
AC_ARG_ENABLE(smb,
  [  --enable-smb            smb input module (default enabled)])
if test "${enable_smb}" != "no"; then
  AC_CHECK_HEADERS(libsmbclient.h,
    [ VLC_ADD_PLUGINS([access_smb])
      VLC_ADD_LIBS([access_smb],[-lsmbclient]) ],
    [ if test -n "${enable_smb}"; then
        AC_MSG_ERROR([cannot find libsmbclient headers])
     fi ])
  AC_CHECK_MEMBER([struct _SMBCCTX.close_fn],
    AC_DEFINE([HAVE__SMBCCTX_CLOSE_FN], 1, [Define if samba has _SMBCCTX.close_fn]),,
    [#include <libsmbclient.h>])
fi

dnl
dnl  libdvbpsi ts demux/mux
dnl
AC_ARG_ENABLE(dvbpsi,
  [  --enable-dvbpsi         dvbpsi ts mux and demux module (default enabled)])
if test "${enable_dvbpsi}" != "no"
then
  AC_ARG_WITH(dvbpsi,
  [    --with-dvbpsi=PATH    libdvbpsi headers and libraries])
  AC_ARG_WITH(dvbpsi,
  [    --with-dvbpsi-tree=PATH libdvbpsi tree for static linking])
  case "${with_dvbpsi}" in
  ""|yes)
    if test -z "${with_dvbpsi_tree}"
    then
      AC_CHECK_HEADERS(dvbpsi/dr.h,
        [ VLC_ADD_PLUGINS([ts])
          if test "${enable_sout}" != "no"; then
            VLC_ADD_PLUGINS([mux_ts])
          fi
          VLC_ADD_LIBS([mux_ts ts dvb],[-ldvbpsi]) ],
        [  AC_MSG_WARN([cannot find libdvbpsi headers]) ],
        [#if defined( HAVE_STDINT_H )
#   include <stdint.h>
#elif defined( HAVE_INTTYPES_H )
#   include <inttypes.h>
#endif
#include <dvbpsi/dvbpsi.h>
#include <dvbpsi/descriptor.h>
#include <dvbpsi/pat.h>
#include <dvbpsi/pmt.h>])
    else
      AC_MSG_CHECKING(for libdvbpsi.a in ${with_dvbpsi_tree})
      real_dvbpsi_tree="`cd ${with_dvbpsi_tree} 2>/dev/null && pwd`"
      if test -z "${real_dvbpsi_tree}"
      then
        dnl  The given directory can't be found
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot cd to ${with_dvbpsi_tree}])
      fi
      if test -f "${real_dvbpsi_tree}/src/.libs/libdvbpsi.a"
      then
        dnl  Use a custom libdvbpsi
        AC_MSG_RESULT(${real_dvbpsi_tree}/src/.libs/libdvbpsi.a)
        VLC_ADD_BUILTINS([ts])
        if test "${enable_sout}" != "no"; then
          VLC_ADD_BUILTINS([mux_ts])
        fi
        VLC_ADD_CPPFLAGS([mux_ts ts dvb],[-I${real_dvbpsi_tree}/src])
        VLC_ADD_LIBS([mux_ts ts dvb],[${real_dvbpsi_tree}/src/.libs/libdvbpsi.a])
      else
        dnl  The given libdvbpsi wasn't built
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot find ${real_dvbpsi_tree}/src/.libs/libdvbpsi.a, make sure you compiled libdvbpsi in ${with_dvbpsi_tree}])
      fi
    fi
  ;;
  no)
    dnl  Compile without dvbpsi
  ;;
  *)
    AC_MSG_CHECKING(for dvbpsi headers in ${with_dvbpsi})
    if test -z "${with_dvbpsi}"
    then
      LDFLAGS_test=""
      CPPFLAGS_test=""
    else
      LDFLAGS_test="-L${with_dvbpsi}/lib"
      CPPFLAGS_test="-I${with_dvbpsi}/include"
    fi
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_test}"
    AC_CHECK_HEADERS([dvbpsi/dr.h],[
      VLC_ADD_PLUGINS([ts])
      if test "${enable_sout}" != "no"; then
        VLC_ADD_PLUGINS([mux_ts])
      fi
      VLC_ADD_CPPFLAGS([mux_ts ts dvb],[${CPPFLAGS_test}])
      VLC_ADD_LIBS([mux_ts ts dvb],[${LDFLAGS_test} -ldvbpsi])

    ],[
      if test -n "${enable_dvbpsi}"
      then
        AC_MSG_ERROR([Could not find libdvbpsi on your system: you may get it from www.videolan.org, you'll need at least version 0.1.1])
      fi
    ])
    CPPFLAGS="${CPPFLAGS_save}"
  ;;
  esac
  AC_CHECK_LIB(dvbpsi, dvbpsi_GenSDTSections, [
    AC_DEFINE(HAVE_DVBPSI_SDT, 1, [Define if you have dvbpsi_GenSDTSections.])
  ], [], [${LIBS_ts}])

fi

dnl
dnl  Video4Linux plugin
dnl
AC_ARG_ENABLE(v4l,
  [  --enable-v4l            Video4Linux input support (default disabled)])
if test "${enable_v4l}" = "yes"
then
  AC_ARG_WITH(v4l,
    [    --with-v4l=PATH       path to a v4l-enabled kernel tree],[],[])
  if test "${with_v4l}" != "no" -a -n "${with_v4l}"
  then
    VLC_ADD_CPPFLAGS([v4l],[-I${with_v4l}/include])
  fi

  CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_v4l}"
  AC_CHECK_HEADERS(linux/videodev.h, [
    VLC_ADD_PLUGINS([v4l])
  ],[])
  CPPFLAGS="${CPPFLAGS_save}"
fi

dnl
dnl  Video4Linux2 plugin
dnl
AC_ARG_ENABLE(v4l2,
  [  --enable-v4l2           Video4Linux2 input support (default disabled)])
if test "${enable_v4l2}" = "yes"
then
  AC_ARG_WITH(v4l2,
    [    --with-v4l2=PATH       path to a v4l2-enabled kernel tree],[],[])
  if test "${with_v4l2}" != "no" -a -n "${with_v4l2}"
  then
    VLC_ADD_CPPFLAGS([v4l2],[-I${with_v4l2}/include])
  fi

  AC_CHECK_HEADER(alsa/asoundlib.h, AC_CHECK_LIB(asound, main, have_v4l2_alsa="true", have_v4l2_alsa="false"),have_v4l2_alsa="false")
  if test "${have_v4l2_alsa}" = "true"
  then
    CFLAGS="${CFLAGS_save}"
    AC_TRY_COMPILE([#define ALSA_PCM_NEW_HW_PARAMS_API
                    #define ALSA_PCM_NEW_SW_PARAMS_API
                    #include <alsa/asoundlib.h>],
       [snd_pcm_hw_params_get_period_time(0,0,0);],
        AC_DEFINE(HAVE_ALSA_NEW_API, 1, Define if ALSA is at least rc4))
    VLC_ADD_LDFLAGS([v4l2],[-lasound -lm -ldl])
    AC_DEFINE(HAVE_ALSA, 1, Define if ALSA is present.)
  fi

  CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_v4l2}"
  AC_CHECK_HEADERS(linux/videodev2.h, [
    VLC_ADD_PLUGINS([v4l2])
  ],[])
  CPPFLAGS="${CPPFLAGS_save}"
fi


dnl
dnl  special access module for Hauppauge PVR cards
dnl
AC_ARG_ENABLE(pvr,
  [  --enable-pvr            PVR cards access module (default disabled)])
if test "${enable_pvr}" = "yes"
then
  VLC_ADD_PLUGINS([pvr])
  AC_ARG_WITH(videodev2,
    [  --with-videodev2=FILE   Location of videodev2.h file (default /usr/include/linux/videodev2.h)],[],[])
  if test "${with_videodev2}" != "no" -a -n "${with_videodev2}"
  then
    AC_DEFINE_UNQUOTED(VIDEODEV2_H_FILE, "${with_videodev2}", [Location of videodev2.h])
  fi
    

  AC_CACHE_CHECK([for new linux/videodev2.h],
      [new_linux_videodev2_h],
      [AC_TRY_COMPILE([
          #include <sys/types.h>
          #   ifdef VIDEODEV2_H_FILE
	  #   include VIDEODEV2_H_FILE
	  #   else
	  #   include <linux/videodev2.h>
	  #   endif
	  ],
          [struct v4l2_ext_controls ctrls; ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG; ],
          new_linux_videodev2_h=yes,
          new_linux_videodev2_h=no)])
  if test "${new_linux_videodev2_h}" != "no"; then
    AC_DEFINE(HAVE_NEW_LINUX_VIDEODEV2_H, 1, [Define if new linux/videodev2.h present])
  fi
fi

dnl
dnl  gnomeVFS access module
dnl
AC_ARG_ENABLE(gnomevfs,
  [  --enable-gnomevfs       GnomeVFS access module (default disabled)])
if test "${enable_gnomevfs}" = "yes" 
then
  PKG_CHECK_MODULES(GNOMEVFS, gnome-vfs-2.0,
    VLC_ADD_LIBS([access_gnomevfs],[$GNOMEVFS_LIBS])
    VLC_ADD_CPPFLAGS([access_gnomevfs],[$GNOMEVFS_CPPFLAGS])
    VLC_ADD_CFLAGS([access_gnomevfs],[$GNOMEVFS_CFLAGS])
    VLC_ADD_PLUGINS([access_gnomevfs]),
    AC_MSG_WARN([GnomeVFS support disabled because GnomeVFS development headers not found]))
fi

dnl Need to test libcdio and libvcdinfo for a number of things. Do it now.
AC_ARG_ENABLE(libcdio,
  [  --enable-libcdio        CD input and control library support (default enabled)])


have_libcdio=no
have_libvcdinfo=no
if test "${enable_libcdio}" != "no"
then
  PKG_CHECK_MODULES(LIBCDIO, libcdio >= 0.78.2,
     have_libcdio=yes
     AC_DEFINE(HAVE_LIBCDIO, 1, 
     [Define if you have libcdio 0.78.2 or greater installed]),
     [AC_MSG_WARN(CD Reading and information library not found)])

  PKG_CHECK_MODULES(VCDINFO, libvcdinfo >= 0.7.22,
     [have_libvcdinfo=yes
     AC_DEFINE(HAVE_VCDINFO, 1, 
     [Define if you have libvcdinfo 0.7.22 or greater installed])],
     [AC_MSG_WARN(VCD information library not found)])
fi

dnl
dnl  VCDX and CDDAX modules
dnl
AC_ARG_ENABLE(cddax,
  [  --enable-cddax          audio CD plugin with CD Text and CD paranoia via libcdio (default disabled)])
 
AC_ARG_ENABLE(libcddb,
  [  --enable-libcddb        CDDB support for libcdio audio CD (default enabled)])
 
if test "${enable_cddax}" = "yes"
then
  if test "$have_libcdio" = "yes"
  then
    AC_DEFINE(HAVE_CDDAX, 1, [Define for the audio CD plugin using libcdio])
    VLC_ADD_LIBS([cddax],[$LIBCDIO_LIBS])
    VLC_ADD_CFLAGS([cddax],[$LIBCDIO_CFLAGS])
    VLC_ADD_PLUGINS([cddax])
    PKG_CHECK_MODULES(LIBCDIO_PARANOIA, libcdio_paranoia >= 0.72, [
    VLC_ADD_LIBS([cddax],[$LIBCDIO_CDDA_LIBS $LIBCDIO_CDDA_LIBS $LIBCDIO_PARANOIA_LIBS])],
    AC_MSG_WARN([CD Paranoia support disabled because no libcdio >= 0.72 found]))
  else 
    AC_MSG_WARN([cddax plugin disabled because ok libcdio library not found or disabled])
    HAVE_CDDAX=no
  fi

  if test "$enable_libcddb" != "no"; then
    PKG_CHECK_MODULES(LIBCDDB, libcddb >= 0.9.5, [
      HAVE_LIBCDDB=yes 
      AC_DEFINE(HAVE_LIBCDDB, 1, [Define this if you have libcddb installed])
      VLC_ADD_LIBS([cddax],[$LIBCDDB_LIBS])
      VLC_ADD_CFLAGS([cddax],[$LIBCDDB_CFLAGS])
      ],
      [AC_MSG_WARN(new enough libcddb not found. CDDB access disabled) 
      HAVE_LIBCDDB=no])
  fi
 
fi

AC_ARG_ENABLE(vcdx,
  [  --enable-vcdx           VCD with navigation via libvcdinfo (default disabled)])
 
if test "${enable_vcdx}" = "yes"
then
  if test "${have_libvcdinfo}" = "yes"
  then
    VLC_ADD_LIBS([vcdx],[$VCDINFO_LIBS])
    VLC_ADD_CFLAGS([vcdx],[$VCDINFO_CFLAGS])
  else 
    AC_MSG_WARN([vcdx plugin disabled because ok libvcdinfo library not found or disabled])
    HAVE_VCDX=no
  fi

  PKG_CHECK_MODULES(LIBCDIO, libiso9660 >= 0.72,
   [VLC_ADD_LIBS([vcdx],[$LIBISO9660_LIBS])
    VLC_ADD_CFLAGS([vcdx],[$LIBISO9660_CFLAGS])],
    [AC_MSG_WARN([vcdx plugin disabled because ok libiso9660 library not found])
    HAVE_VCDX=no])

  if test "$have_libvcdinfo" = "yes"
  then
    AC_DEFINE(HAVE_VCDX, 1, 
    [Define for the VCD plugin using libcdio/libvcdinfo])
    VLC_ADD_LIBS([vcdx],[$VCDINFO_LIBS])
    VLC_ADD_CFLAGS([vcdx],[$VCDINFO_CFLAGS])
    VLC_ADD_PLUGINS([vcdx])
  else 
    AC_MSG_WARN([vcdx plugin disabled because ok libvcdinfo library not found])
    HAVE_VCDX=no
  fi
fi

dnl
dnl  Built-in CD-DA and VCD module
dnl
AC_ARG_ENABLE(cdda,           
  [  --enable-cdda           audio CD via built-in VCD (default enabled)])
 
AC_ARG_ENABLE(vcd,
  [  --enable-vcd            built-in VCD (default enabled)])

if test "${enable_vcd}" != "no"
then
  AC_MSG_CHECKING(for cdrom_msf0 in linux/cdrom.h)
  AC_EGREP_HEADER(cdrom_msf0,linux/cdrom.h,[
    AC_MSG_RESULT(yes)
    VLC_ADD_PLUGINS([vcd cdda])
  ],[
    AC_MSG_RESULT(no)
  ])

  AC_MSG_CHECKING(for scsireq in sys/scsiio.h)
  AC_EGREP_HEADER(scsireq,sys/scsiio.h,[
    AC_MSG_RESULT(yes)
    VLC_ADD_PLUGINS([vcd cdda])
    AC_DEFINE(HAVE_SCSIREQ_IN_SYS_SCSIIO_H, 1, For NetBSD VCD support)
  ],[
    AC_MSG_RESULT(no)
  ])

  AC_MSG_CHECKING(for ioc_toc_header in sys/cdio.h)
  AC_EGREP_HEADER(ioc_toc_header ,sys/cdio.h,[
    AC_MSG_RESULT(yes)
    VLC_ADD_PLUGINS([vcd cdda])
    AC_DEFINE(HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H, 1, For FreeBSD VCD support)
  ],[
    AC_MSG_RESULT(no)
  ])

  if test "${SYS}" = "bsdi" -o "${SYS}" = "mingw32"
  then
    VLC_ADD_PLUGINS([vcd cdda])
  fi

  if test "${SYS}" = "darwin"
  then
    VLC_ADD_PLUGINS([vcd cdda])
    VLC_ADD_LDFLAGS([vcd vcdx cdda cddax],[-Wl,-framework,IOKit,-framework,CoreFoundation])
    VLC_ADD_LIBS([vcdx cddax cdda],[-liconv])
  fi

  if test "$enable_libcddb" != "no"; then
    PKG_CHECK_MODULES(LIBCDDB, libcddb >= 0.9.5, [
      HAVE_LIBCDDB=yes 
      AC_DEFINE(HAVE_LIBCDDB, 1, [Define this if you have libcddb installed])
      VLC_ADD_LIBS([cdda],[$LIBCDDB_LIBS])
      VLC_ADD_CFLAGS([cdda],[$LIBCDDB_CFLAGS])
      ],:
      [AC_MSG_WARN(new enough libcddb not found. CDDB access disabled) 
      HAVE_LIBCDDB=no])
  fi
fi

dnl
dnl  DVB-S/DVB-T/DVB-C satellite/teresterial/cable input using v4l2
dnl
AC_ARG_ENABLE(dvb,
  [  --enable-dvb            DVB-S/T/C card support (default disabled)])

if test "${enable_dvb}" = "yes"
then
  AC_ARG_WITH(dvb,
  [    --with-dvb=PATH       path to a dvb- and v4l2-enabled kernel tree],[],[])
  if test "${with_dvb}" != "no" -a -n "${with_dvb}"
  then
    VLC_ADD_CFLAGS([dvb],[-I${with_dvb}/include])
  fi
  CPPFLAGS="${CPPFLAGS_save} -I${with_dvb}/include"
  AC_CHECK_HEADERS(linux/dvb/version.h linux/dvb/frontend.h, [
    if test -z "${with_dvbpsi_tree}"
    then
      VLC_ADD_PLUGINS([dvb])
    else
      VLC_ADD_BUILTINS([dvb])
    fi
  ],[AC_MSG_WARN(linux-dvb headers not found, dvb disabled)])
  CPPFLAGS="${CPPFLAGS_save}"
fi

dnl
dnl  Screen capture module
dnl
AC_ARG_ENABLE(screen,
  [  --enable-screen         Screen capture support (default enabled)])
if test "${enable_screen}" != "no"; then
  if test "${SYS}" = "darwin"; then
    AC_CHECK_HEADERS(ApplicationServices/ApplicationServices.h, [
      VLC_ADD_PLUGINS([screen])
      VLC_ADD_LDFLAGS([screen],[-Wl,-framework,ApplicationServices])
    ])
  elif test "${SYS}" = "mingw32"; then
    VLC_ADD_PLUGINS([screen])
    VLC_ADD_LIBS([screen],[-lgdi32])
  elif test "${SYS}" = "mingwce"; then
    CPPFLAGS="${CPPFLAGS_save}"
  elif test "${SYS}" = "beos"; then
    VLC_ADD_PLUGINS([screen])
    VLC_ADD_CXXFLAGS([screen],[])
    VLC_ADD_LIBS([screen],[-lbe])
  else
    CPPFLAGS="${CPPFLAGS_save} ${X_CFLAGS}"
    AC_CHECK_HEADERS(X11/Xlib.h, [
      VLC_ADD_PLUGINS([screen])
      VLC_ADD_LIBS([screen],[${X_LIBS} ${X_PRE_LIBS} -lX11])
      VLC_ADD_CPPFLAGS([screen],[${X_CFLAGS}])
    ])
    CPPFLAGS="${CPPFLAGS_save}"
  fi
fi

dnl
dnl  ipv6 plugin - not for QNX yet
dnl
have_ipv6=no
AC_CHECK_FUNCS(inet_pton,[have_ipv6=yes],[
  AC_CHECK_LIB(nsl,inet_pton, [have_ipv6=yes])
])

AS_IF([test "${have_ipv6}" = "yes"], [
  AC_DEFINE(HAVE_INET_PTON, 1, [Define to 1 if you have inet_pton().])])


AC_CHECK_FUNCS(inet_ntop,[
  AC_DEFINE(HAVE_INET_NTOP, 1, [Define to 1 if you have inet_ntop().])])


dnl
dnl  ogg demux plugin
dnl
AC_ARG_ENABLE(ogg,
  [  --enable-ogg            Ogg demux support (default enabled)])
if test "${enable_ogg}" != "no"
then
  AC_ARG_WITH(ogg-tree,
  [    --with-ogg-tree=PATH  ogg tree for static linking])
  if test -n "${with_ogg_tree}"
  then
    AC_MSG_CHECKING(for libogg.a in ${with_ogg_tree})
    real_ogg_tree="`cd ${with_ogg_tree} 2>/dev/null && pwd`"
    if test -z "${real_ogg_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_ogg_tree}])
    fi
    if test -f "${real_ogg_tree}/src/.libs/libogg.a"
    then
      dnl  Use a custom ogg
      AC_MSG_RESULT(${real_ogg_tree}/src/.libs/libogg.a)
      VLC_ADD_PLUGINS([ogg])
      if test "${enable_sout}" != "no"; then
        VLC_ADD_PLUGINS([mux_ogg])
      fi
      VLC_ADD_LIBS([ogg mux_ogg speex vorbis],[${real_ogg_tree}/src/.libs/libogg.a])
      VLC_ADD_CFLAGS([ogg mux_ogg speex vorbis],[-I${real_ogg_tree}/include])
    else
      dnl  The given ogg wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_ogg_tree}/src/.libs/libogg.a, make sure you compiled ogg in ${with_ogg_tree}])
    fi
  else
    AC_CHECK_HEADERS(ogg/ogg.h, [
      AC_CHECK_LIB( ogg, oggpack_read, [
        VLC_ADD_PLUGINS([ogg])
        if test "${enable_sout}" != "no"; then
          VLC_ADD_PLUGINS([mux_ogg])
        fi
        VLC_ADD_LIBS([ogg mux_ogg],[-logg])])
     ],[])
  fi
fi

dnl
dnl  matroska demux plugin
dnl
AC_ARG_ENABLE(mkv,
  [  --enable-mkv            Matroska demux support (default enabled)])
if test "${enable_mkv}" != "no" -a "${CXX}" != ""; then
  AC_LANG_PUSH(C++)
  AC_CHECK_HEADERS(ebml/EbmlVersion.h, [
    AC_MSG_CHECKING(for libebml version >= 0.7.6)
    AC_EGREP_CPP(yes,
      [#include <ebml/EbmlVersion.h>
       #ifdef LIBEBML_VERSION
       #if LIBEBML_VERSION >= 0x000706
       yes
       #endif
       #endif],
      [AC_MSG_RESULT([yes])
        AC_CHECK_HEADERS(matroska/KaxVersion.h, [
          AC_MSG_CHECKING(for libmatroska version >= 0.7.7)
          AC_EGREP_CPP(yes,
            [#include <matroska/KaxVersion.h>
             #ifdef LIBMATROSKA_VERSION
             #if LIBMATROSKA_VERSION >= 0x000705
             yes
             #endif
             #endif],
            [AC_MSG_RESULT([yes])
              AC_CHECK_HEADERS(matroska/KaxAttachments.h)
              VLC_ADD_CXXFLAGS([mkv],[])
              if test "${SYS}" = "darwin"; then
                VLC_ADD_CXXFLAGS([mkv],[-O1])
              fi
              AC_CHECK_LIB(ebml_pic, main, [
                VLC_ADD_PLUGINS([mkv])
                VLC_ADD_LIBS([mkv],[-lmatroska -lebml_pic])
              ],
                AC_CHECK_LIB(ebml, main, [
                  VLC_ADD_PLUGINS([mkv])
                  VLC_ADD_LIBS([mkv],[-lmatroska -lebml])
                ]) 
	      )
            ],
            [AC_MSG_RESULT([no])
              AC_MSG_ERROR([Your libmatroska is too old: you may get a more recent one from http://dl.matroska.org/downloads/libmatroska/. Alternatively you can use --disable-mkv to disable the matroska plugin.])
          ])
        ])
      ],
      [AC_MSG_RESULT([no])
        AC_MSG_ERROR([Your libebml is too old: you may get a more recent one from http://dl.matroska.org/downloads/libebml/. Alternatively you can use --disable-mkv to disable the matroska plugin.])
    ])
  ])
  AC_LANG_POP(C++)
fi

dnl
dnl  modplug demux plugin
dnl
AC_ARG_ENABLE(mod,
  [  --enable-mod            Mod demux support (default enabled)])
if test "${enable_mod}" != "no"
then
  AC_ARG_WITH(mod-tree,
  [    --with-mod-tree=PATH mod tree for static linking])
  if test -n "${with_mod_tree}"
  then
    AC_MSG_CHECKING(for libmodplug.a in ${with_mod_tree})
    real_mod_tree="`cd ${with_mod_tree} 2>/dev/null && pwd`"
    if test -z "${real_mod_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_mod_tree}])
    fi
    if test -f "${real_mod_tree}/src/.libs/libmodplug.a"
    then
      dnl  Use a custom mod
      AC_MSG_RESULT(${real_mod_tree}/src/.libs/libmodplug.a)
      VLC_ADD_PLUGINS([mod])
      VLC_ADD_LIBS([mod],[${real_mod_tree}/src/.libs/libmodplug.a -lstdc++])
      VLC_ADD_CXXFLAGS([mod],[-I${real_mod_tree}/include])
    else
      dnl  The given mod wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_mod_tree}/src/.libs/libmodplug.a, make sure you compiled mod in ${with_mod_tree}])
    fi
  else
    AC_CHECK_HEADERS(libmodplug/modplug.h, [
      VLC_ADD_PLUGINS([mod])
      VLC_ADD_CXXFLAGS([mod],[])
      VLC_ADD_LIBS([mod],[-lmodplug -lstdc++])])
  fi
fi

dnl
dnl  mpc demux plugin
dnl
AC_ARG_ENABLE(mpc,
  [  --enable-mpc            Mpc demux support (default enabled)])
if test "${enable_mpc}" != "no"
then
  AC_CHECK_HEADERS(mpcdec/mpcdec.h, [
    VLC_ADD_PLUGINS([mpc])
    VLC_ADD_LIBS([mpc],[-lmpcdec])])
fi

dnl
dnl  game music emu demux plugin
dnl
AC_ARG_ENABLE(gme,
  [  --enable-gme            Game Music Emu demux support (default enabled)])
if test "${enable_gme}" != "no" -a "${CXX}" != "";
then
  AC_LANG_PUSH(C++)
  AC_ARG_WITH(gme-tree,
  [    --with-gme-tree=PATH gme tree for static linking])
  if test -n "${with_gme_tree}"
  then
    AC_MSG_CHECKING(for libgme.a in ${with_mod_tree})
    real_gme_tree="`cd ${with_gme_tree} 2>/dev/null && pwd`"
    if test -z "${real_gme_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_gme_tree}])
    fi
    if test -f "${real_gme_tree}/gme/libgme.a"
    then
      dnl  Use a custom gme
      AC_MSG_RESULT(${real_gme_tree}/gme/libgme.a)
      VLC_ADD_PLUGINS([gme])
      VLC_ADD_LIBS([gme],[${real_gme_tree}/gme/libgme.a])
      VLC_ADD_CXXFLAGS([gme],[-I${real_gme_tree}/gme])
    else
      dnl  The given gme wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_mod_tree}/gme/libgme.a, make sure you compiled gme in ${with_gme_tree}])
    fi
  else
      AC_MSG_WARN([only static linking is available, you must provide a gme-tree])
  fi
  AC_LANG_POP(C++)
fi

dnl
dnl  Codec plugins
dnl

AC_ARG_WITH(,[Codec plugins:])

dnl
dnl  mad plugin
dnl
AC_ARG_ENABLE(mad,
  [  --enable-mad            libmad module (default enabled)])
if test "${enable_mad}" != "no"
then
  AC_ARG_WITH(mad,
    [    --with-mad=PATH       path to libmad],[],[])
  if test "${with_mad}" != "no" -a -n "${with_mad}"
  then
    VLC_ADD_CPPFLAGS([mpgatofixed32],[-I${with_mad}/include])
    VLC_ADD_LIBS([mpgatofixed32],[-L${with_mad}/lib])
  fi

  AC_ARG_WITH(mad-tree,
    [    --with-mad-tree=PATH   mad tree for static linking],[],[])
  if test "${with_mad_tree}" != "no" -a -n "${with_mad_tree}"
  then
    real_mad_tree="`cd ${with_mad_tree} 2>/dev/null && pwd`"
    if test -z "${real_mad_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([${with_mad_tree} directory doesn't exist])
    fi
    dnl  Use a custom libmad
    AC_MSG_CHECKING(for mad.h in ${real_mad_tree})
    if test -f ${real_mad_tree}/mad.h
    then
      AC_MSG_RESULT(yes)
      VLC_ADD_CPPFLAGS([mpgatofixed32],[-I${real_mad_tree}])
      VLC_ADD_LIBS([mpgatofixed32],[-L${real_mad_tree}/.libs])
      LDFLAGS="${LDFLAGS_save} ${LIBS_mpgatofixed32}"
      AC_CHECK_LIB(mad, mad_bit_init, [
        VLC_ADD_BUILTINS([mpgatofixed32])
        VLC_ADD_LIBS([mpgatofixed32],[-lmad])
        ],[ AC_MSG_ERROR([the specified tree hasn't been compiled ])
      ],[])
      LDFLAGS="${LDFLAGS_save}"
    else
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([the specified tree doesn't have mad.h])
    fi
  else
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_mpgatofixed32}"
    LDFLAGS="${LDFLAGS_save} ${LIBS_mpgatofixed32}"
    AC_CHECK_HEADERS(mad.h, ,
      [ AC_MSG_ERROR([Could not find libmad on your system: you may get it from http://www.underbit.com/products/mad/. Alternatively you can use --disable-mad to disable the mad plugin.]) ])
    AC_CHECK_LIB(mad, mad_bit_init, [
      VLC_ADD_PLUGINS([mpgatofixed32])
      VLC_ADD_LIBS([mpgatofixed32],[-lmad])],
      [ AC_MSG_ERROR([Cannot find libmad library...]) ])
    CPPFLAGS="${CPPFLAGS_save}"
    LDFLAGS="${LDFLAGS_save}"
  fi
fi

dnl
dnl   libid3tag support (FIXME!!! doesn't work with new input)
dnl
AC_ARG_ENABLE( id3tag,
  [  --disable-id3tag id3tag metadata reader plugin (default enabled)])
AS_IF([test "${enable_id3tag}" != "no"], [
  AC_CHECK_HEADERS(id3tag.h, [
    AC_CHECK_HEADERS(zlib.h, [
      VLC_ADD_LIBS([id3tag],[-lid3tag -lz])
      VLC_ADD_PLUGINS([id3tag])
    ])
  ])
])

dnl
dnl  ffmpeg decoder/demuxer plugin
dnl
dnl we try to find ffmpeg using : 1- given tree, 2- pkg-config
dnl                               3- default place,

AC_ARG_ENABLE(ffmpeg,
[  --enable-ffmpeg         ffmpeg codec (default enabled)])
if test "${enable_ffmpeg}" != "no"
then

dnl Those options have to be here because the .pc can be bogus for ffmpeg previous nov 05

 AC_ARG_WITH(ffmpeg-mp3lame,
   [    --with-ffmpeg-mp3lame specify if ffmpeg has been compiled with mp3lame support],
   [
     if test "$with_ffmpeg_mp3lame" = "yes";	then
        VLC_ADD_LIBS([ffmpeg],[-lmp3lame])
     fi])

 AC_ARG_WITH(ffmpeg-faac,
   [    --with-ffmpeg-faac    specify if ffmpeg has been compiled with faac support],
   [
     if test "$with_ffmpeg_faac" = "yes"; then
        VLC_ADD_LIBS([ffmpeg],[-lfaac])
     fi])

 AC_ARG_WITH(ffmpeg-dts,
   [    --with-ffmpeg-dts     specify if ffmpeg has been compiled with dts support],
   [
     if test "$with_ffmpeg_dts" = "yes"; then
             LDFLAGS="${LDFLAGS_save} ${LDFLAGS_ffmpeg} ${LIBS_ffmpeg}"
             AC_CHECK_LIB(dts_pic, dts_free, 
               [ VLC_ADD_LIBS([ffmpeg],[-ldts_pic]) ],
               [ VLC_ADD_LIBS([ffmpeg],[-ldts]) ])
             LDFLAGS="${LDFLAGS_save}"
     fi])

 AC_ARG_WITH(ffmpeg-vorbis,
   [    --with-ffmpeg-vorbis    specify if ffmpeg has been compiled with vorbis support],
   [
     if test "$with_ffmpeg_vorbis" = "yes"; then
        VLC_ADD_LIBS([ffmpeg],[-lvorbisenc -lvorbis])
     fi])

 AC_ARG_WITH(ffmpeg-theora,
   [    --with-ffmpeg-theora    specify if ffmpeg has been compiled with theora support],
   [
     if test "$with_ffmpeg_theora" = "yes"; then
        VLC_ADD_LIBS([ffmpeg],[-ltheora])
     fi])

 AC_ARG_WITH(ffmpeg-ogg,
   [    --with-ffmpeg-ogg    specify if ffmpeg has been compiled with ogg support],
   [
     if test "$with_ffmpeg_ogg" = "yes"; then
        VLC_ADD_LIBS([ffmpeg],[-logg])
     fi])

 AC_ARG_WITH(ffmpeg-zlib,
   [    --with-ffmpeg-zlib    specify if ffmpeg has been compiled with zlib support],
   [
     if test "$with_ffmpeg_zlib" = "yes"; then
        VLC_ADD_LIBS([ffmpeg],[-lz])
     fi])

 AC_ARG_WITH(ffmpeg-amrnb,
   [    --with-ffmpeg-amrnb specify if ffmpeg has been compiled with libamrnb support],
   [
     if test "$with_ffmpeg_amrnb" = "yes";	then
        VLC_ADD_LIBS([ffmpeg],[-lamrnb])
     fi])

 AC_ARG_WITH(ffmpeg-amrwb,
   [    --with-ffmpeg-amrwb specify if ffmpeg has been compiled with libamrwb support],
   [
     if test "$with_ffmpeg_amrwb" = "yes";	then
        VLC_ADD_LIBS([ffmpeg],[-lamrwb])
     fi])

 AC_ARG_WITH(ffmpeg-x264,
   [    --with-ffmpeg-x264 specify if ffmpeg has been compiled with libx264 support],
   [
     if test "$with_ffmpeg_x264" = "yes";	then
        VLC_ADD_LIBS([ffmpeg],[-lx264])
     fi])

 AC_ARG_WITH(ffmpeg-a52,
   [    --with-ffmpeg-a52 specify if ffmpeg has been compiled with liba52 support],
   [
     if test "$with_ffmpeg_a52" = "yes";	then
        VLC_ADD_LIBS([ffmpeg],[-la52])
     fi])

 dnl
 dnl test for --with-ffmpeg-tree
 dnl
 AC_ARG_WITH(ffmpeg-tree,
   [    --with-ffmpeg-tree=PATH ffmpeg tree for static linking])

 if test "${with_ffmpeg_tree}" != "no" -a -n "${with_ffmpeg_tree}"; then
   AC_MSG_CHECKING(for libavcodec.a in ${with_ffmpeg_tree})
   real_ffmpeg_tree="`cd ${with_ffmpeg_tree} 2>/dev/null && pwd`"
   if test -z "${real_ffmpeg_tree}"; then
     dnl  The given directory can't be found
     AC_MSG_RESULT(no)
     AC_MSG_ERROR([cannot cd to ${with_ffmpeg_tree}])
   fi
   if ! test -f "${real_ffmpeg_tree}/libavcodec/libavcodec.a"; then
     dnl  The given libavcodec wasn't built
     AC_MSG_RESULT(no)
     AC_MSG_ERROR([cannot find ${real_ffmpeg_tree}/libavcodec/libavcodec.a, make sure you compiled libavcodec in ${with_ffmpeg_tree}])
   fi
   if ! fgrep -s "pp_get_context" "${real_ffmpeg_tree}/libavcodec/libavcodec.a"; then
     if test -f "${real_ffmpeg_tree}/libavcodec/libpostproc/libpostproc.a"; then
       VLC_ADD_LIBS([ffmpeg],[-L${real_ffmpeg_tree}/libavcodec/libpostproc ${real_ffmpeg_tree}/libavcodec/libpostproc/libpostproc.a])	
     elif test -f "${real_ffmpeg_tree}/libpostproc/libpostproc.a"; then 
        VLC_ADD_LIBS([ffmpeg],[-L${real_ffmpeg_tree}/libpostproc ${real_ffmpeg_tree}/libpostproc/libpostproc.a])
        VLC_ADD_CPPFLAGS([ffmpeg stream_out_switcher],[-I${real_ffmpeg_tree}])
     else
       dnl  The given libavcodec wasn't built with --enable-pp
       AC_MSG_RESULT(no)
       AC_MSG_ERROR([cannot find libpostproc.a in ${real_ffmpeg_tree}/[libavcodec/]libpostproc/. Make sure you configured ffmpeg with --enable-pp])
     fi
   fi
   dnl  Use a custom libffmpeg
   AC_MSG_RESULT(${real_ffmpeg_tree}/libavcodec/libavcodec.a)

   dnl Use pkg-config to look for the ldflags in libav*-uninstalled.pc
   PKG_CONFIG_PATH_save="$PKG_CONFIG_PATH"
   PKG_CONFIG_LIBDIR_save="$PKG_CONFIG_LIBDIR"
   export PKG_CONFIG_PATH=""
   export PKG_CONFIG_LIBDIR="${real_ffmpeg_tree}"

   PKG_CHECK_MODULES( [FFMPEG],[libavcodec libavformat libpostproc], [
        VLC_ADD_CPPFLAGS([ffmpeg stream_out_switcher], ${FFMPEG_CFLAGS})
        VLC_ADD_LIBS([ffmpeg stream_out_switcher], ${FFMPEG_LIBS})]
      ,[])
   AS_IF([test -n "${PKG_CONFIG_PATH_save}"],[
         export PKG_CONFIG_PATH="${PKG_CONFIG_PATH_save}"
   ],[
        unset PKG_CONFIG_PATH
   ])
   AS_IF([test -n "${PKG_CONFIG_LIBDIR_save}"],[
        export PKG_CONFIG_LIBDIR="${PKG_CONFIG_LIBDIR_save}"
   ],[
        unset PKG_CONFIG_LIBDIR
   ])

   VLC_ADD_BUILTINS([ffmpeg])
   if test "${enable_sout}" != "no" -a "${enable_switcher}" = "yes" ; then
     VLC_ADD_BUILTINS([stream_out_switcher])
   fi

   if test -f "${real_ffmpeg_tree}/libswscale/libswscale.a"; then
     AC_DEFINE(HAVE_LIBSWSCALE_TREE, 1, [Define if you have ffmpeg's libswscale.])
     VLC_ADD_LIBS([ffmpeg],[-L${real_ffmpeg_tree}/libswscale ${real_ffmpeg_tree}/libswscale/libswscale.a])
     VLC_ADD_CPPFLAGS([ffmpeg stream_out_switcher],[-I${real_ffmpeg_tree}/libswscale])
   fi

    if test -f "${real_ffmpeg_tree}/libavformat/libavformat.a"; then
      AC_DEFINE(HAVE_LIBAVFORMAT_TREE, 1, [Define if you have ffmpeg's libavformat.])
      VLC_ADD_LIBS([ffmpeg],[-L${real_ffmpeg_tree}/libavformat -lavformat -lz])
      VLC_ADD_CPPFLAGS([ffmpeg],[-I${real_ffmpeg_tree}/libavformat])
    fi
else


dnl Trying with pkg-config
   PKG_CHECK_MODULES(FFMPEG,[libavcodec, libavformat],
    [
     VLC_SAVE_FLAGS
     CPPFLAGS="${CPPFLAGS} ${FFMPEG_CFLAGS}"
     CFLAGS="${CFLAGS} ${FFMPEG_CFLAGS}"
     AC_CHECK_HEADERS(ffmpeg/avcodec.h, [], [AC_MSG_ERROR([Missing header file ffmpeg/avcodec.h.])] )
     AC_CHECK_HEADERS(ffmpeg/avformat.h, [], [AC_MSG_ERROR([Missing header file ffmpeg/avformat.h.])] )
     AC_CHECK_HEADERS(ffmpeg/avutil.h, [], [AC_MSG_ERROR([Missing header file ffmpeg/avutil.h.])] )
     dnl newer ffmpeg have a separate libpostproc
     PKG_CHECK_MODULES(POSTPROC, libpostproc,[
       VLC_ADD_CFLAGS([ffmpeg],[${POSTPROC_CFLAGS}]) 
       VLC_ADD_LIBS([ffmpeg],[${POSTPROC_LIBS}]) 
       ],[ true ])
     CPPFLAGS="${CPPFLAGS} ${POSTPROC_CFLAGS}"
     CFLAGS="${CFLAGS} ${POSTPROC_CFLAGS}"
     AC_CHECK_HEADERS(postproc/postprocess.h, [], [AC_MSG_ERROR([Missing header file postproc/postprocess.h.])] )
     if test "${SYS}" = "darwin"; then
        VLC_ADD_BUILTINS([ffmpeg])
     else
        VLC_ADD_PLUGINS([ffmpeg])
     fi
     if test "${enable_sout}" != "no" -a "${enable_switcher}" = "yes"; then
        if test "${SYS}" = "darwin"; then
            VLC_ADD_BUILTINS([stream_out_switcher])
        else
            VLC_ADD_PLUGINS([stream_out_switcher])
        fi
     fi
     VLC_ADD_CFLAGS([ffmpeg stream_out_switcher],[${FFMPEG_CFLAGS}])
     VLC_ADD_LIBS([ffmpeg stream_out_switcher],[${FFMPEG_LIBS}])

     dnl even newer ffmpeg has a libswscale
     PKG_CHECK_MODULES(SWSCALE,[libswscale],[
       VLC_ADD_CFLAGS([ffmpeg],[${SWSCALE_CFLAGS}])
       VLC_ADD_LIBS([ffmpeg],[${SWSCALE_LIBS}])
dnl    AC_CHECK_HEADERS(ffmpeg/swscale.h, [], [AC_MSG_ERROR([Missing header file ffmpeg/swscale.h.])])
       AC_CHECK_LIB(swscale, sws_getContext,
         [AC_CHECK_HEADERS([ffmpeg/avutil.h, ffmpeg/swscale.h])],[],[-lavutil -lm])
       ],[ true ])
     VLC_RESTORE_FLAGS
    ],[

    dnl
    dnl last chance: at the default place
    dnl
      CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_ffmpeg}"
      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_ffmpeg} ${LIBS_ffmpeg}"
      AC_CHECK_HEADERS(ffmpeg/avcodec.h, [], [AC_MSG_ERROR([Missing header file ffmpeg/avcodec.h.])] )
      AC_CHECK_HEADERS(ffmpeg/avformat.h)
      AC_CHECK_HEADERS(ffmpeg/avutil.h)
      AC_CHECK_HEADERS(postproc/postprocess.h, [], [AC_MSG_ERROR([Missing header file postproc/postprocess.h.])] )

      AC_CHECK_LIB(avutil, av_crc_init, [
        VLC_ADD_LIBS([ffmpeg],[-lavutil])
        LDAVUTIL="-lavutil"])

      AC_CHECK_LIB(avcodec, avcodec_init, [
        VLC_ADD_BUILTINS([ffmpeg])
        VLC_ADD_LIBS([ffmpeg],[-lavcodec])
        if test "${enable_sout}" != "no" -a "${enable_switcher}" = "yes"; then
            VLC_ADD_BUILTINS([stream_out_switcher])
        fi],
         [ AC_MSG_ERROR([Could not find ffmpeg on your system: you may get it from http://ffmpeg.sf.net/ (svn version is recommended). Alternatively you can use --disable-ffmpeg to disable the ffmpeg plugins.]) ], [$LDAVUTIL])

      AC_CHECK_LIB(postproc, pp_postprocess, [
        VLC_ADD_LIBS([ffmpeg],[-lpostproc])],
      AC_MSG_ERROR([Could not find libpostproc inside FFmpeg. You should configure FFmpeg with --enable-gpl --enable-pp.]),
        [$LDAVUTIL])

      AC_CHECK_LIB(avformat, av_open_input_stream, [
        VLC_ADD_LIBS([ffmpeg],[-lavformat -lz]) ], [], [-lavcodec -lz $LDAVUTIL])
      LDFLAGS="${LDFLAGS_save}"
      CPPFLAGS="${CPPFLAGS_save}"

      AC_CHECK_LIB(swscale, sws_getContext, [
        AC_CHECK_HEADERS(ffmpeg/swscale.h)
        VLC_ADD_LIBS([ffmpeg],[-lswscale]) ], [], [-lavcodec $LDAVUTIL])
      LDFLAGS="${LDFLAGS_save}"
      CPPFLAGS="${CPPFLAGS_save}"
    ])
  fi
fi

dnl
dnl  ffmpegaltivec plugin
dnl
AC_ARG_ENABLE(ffmpegaltivec,
[  --enable-ffmpegaltivec  ffmpegaltivec codec (DO NOT USE)])
if test "${enable_ffmpegaltivec}" = "yes"
then
  if test "${with_ffmpeg_tree}" != "no" -a -n "${with_ffmpeg_tree}"; then
    AC_MSG_CHECKING(for libavcodecaltivec.a in ${with_ffmpeg_tree})
    real_ffmpeg_tree="`cd ${with_ffmpeg_tree} 2>/dev/null && pwd`"
    if test -z "${real_ffmpeg_tree}"; then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_ffmpeg_tree}])
    fi
    if ! test -f "${real_ffmpeg_tree}/libavcodec/libavcodecaltivec.a"; then
      dnl  The given libavcodecaltivec wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_ffmpeg_tree}/libavcodec/libavcodecaltivec.a, make sure you compiled libavcodecaltivec in ${with_ffmpeg_tree}])
    fi
    if ! fgrep -s "pp_get_context" "${real_ffmpeg_tree}/libavcodec/libavcodecaltivec.a"; then
      dnl  The given libavcodecaltivec wasn't built with --enable-pp
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([${real_ffmpeg_tree}/libavcodec/libavcodecaltivec.a was not compiled with postprocessing support, make sure you configured ffmpeg with --enable-pp])
    fi
    dnl  Use a custom libffmpeg
    AC_MSG_RESULT(${real_ffmpeg_tree}/libavcodec/libavcodecaltivec.a)
    VLC_ADD_BUILTINS([ffmpegaltivec])
    VLC_ADD_LIBS([ffmpegaltivec],[-L${real_ffmpeg_tree}/libavcodec -lavcodecaltivec])
    VLC_ADD_CPPFLAGS([ffmpeg],[-DNO_ALTIVEC_IN_FFMPEG])
    VLC_ADD_CPPFLAGS([ffmpegaltivec],[-I${real_ffmpeg_tree}/libavcodec -I${real_ffmpeg_tree}/libavformat])

    if test -f "${real_ffmpeg_tree}/libavformat/libavformat.a"; then
      AC_DEFINE(HAVE_LIBAVFORMAT_TREE, 1, [Define if you have ffmpeg's libavformat.])
      VLC_ADD_LIBS([ffmpegaltivec],[-L${real_ffmpeg_tree}/libavformat -lavformataltivec -lz])
      VLC_ADD_CPPFLAGS([ffmpegaltivec],[-I${real_ffmpeg_tree}/libavformat])
    fi
    VLC_ADD_LIBS([stream_out_switcher],[-L${real_ffmpeg_tree}/libavcodec])
    VLC_ADD_CPPFLAGS([stream_out_switcher],[-I${real_ffmpeg_tree}/libavcodec -I${real_ffmpeg_tree}/libavformat])
  fi
fi

dnl
dnl  faad decoder plugin
dnl
AC_ARG_ENABLE(faad,
[  --enable-faad           faad codec (default disabled)])
if test "${enable_faad}" = "yes"
then
  AC_ARG_WITH(faad-tree,
  [    --with-faad-tree=PATH faad tree for static linking])
  if test -n "${with_faad_tree}"
  then
    AC_MSG_CHECKING(for libfaad.a in ${with_faad_tree})
    real_faad_tree="`cd ${with_faad_tree} 2>/dev/null && pwd`"
    if test -z "${real_faad_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_faad_tree}])
    fi
    if test -f "${real_faad_tree}/libfaad/.libs/libfaad.a"
    then
      dnl  Use a custom faad
      AC_MSG_RESULT(${real_faad_tree}/libfaad/.libs/libfaad.a)
      VLC_ADD_BUILTINS([faad])
      VLC_ADD_LIBS([faad],[${real_faad_tree}/libfaad/.libs/libfaad.a])
      VLC_ADD_CPPFLAGS([faad],[-I${real_faad_tree}/include])
    else
      dnl  The given libfaad wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_faad_tree}/libfaad/.libs/libfaad.a, make sure you compiled libfaad in ${with_faad_tree}])
    fi
  else
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_faad}"
    LDFLAGS="${LDFLAGS_save} ${LIBS_faad}"
    AC_CHECK_HEADERS(faad.h, ,
      [ AC_MSG_ERROR([Cannot find development headers for libfaad...]) ])
    AC_CHECK_LIB(faad, faacDecOpen, [
      VLC_ADD_PLUGINS([faad])
      VLC_ADD_LIBS([faad],[-lfaad]) ],
      AC_CHECK_LIB(faad, NeAACDecOpen, [
        VLC_ADD_PLUGINS([faad])
        VLC_ADD_LIBS([faad],[-lfaad]) ],
        [ AC_MSG_ERROR([Cannot find libfaad library...]) ]))
    LDFLAGS="${LDFLAGS_save}"
    CPPFLAGS="${CPPFLAGS_save}"
  fi
fi

dnl
dnl twolame encoder plugin
dnl
AC_ARG_ENABLE(twolame,
[  --enable-twolame        twolame codec (default disabled)])
if test "${enable_twolame}" = "yes"
then
  AC_ARG_WITH(twolame-tree,
  [    --with-twolame-tree=PATH twolame tree for static linking])
  if test -n "${with_twolame_tree}"
  then
    AC_MSG_CHECKING(for libtwolame.a in ${with_twolame_tree})
    real_twolame_tree="`cd ${with_twolame_tree} 2>/dev/null && pwd`"
    if test -z "${real_twolame_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_twolame_tree}])
    fi
    if test -f "${real_twolame_tree}/libtwolame/.libs/libtwolame.a"
    then
      dnl  Use a custom twolame
      AC_MSG_RESULT(${real_twolame_tree}/libtwolame/.libs/libtwolame.a)
      VLC_ADD_BUILTINS([twolame])
      VLC_ADD_LIBS([twolame],[${real_twolame_tree}/libtwolame/.libs/libtwolame.a])
      VLC_ADD_CPPFLAGS([twolame],[-I${real_twolame_tree}/libtwolame -DLIBTWOLAME_STATIC])
    else
      dnl  The given libtwolame wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_twolame_tree}/libtwolame/.libs/libtwolame.a, make sure you compiled libtwolame in ${with_twolame_tree}])
    fi
  else
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_twolame} -DLIBTWOLAME_STATIC"
    LDFLAGS="${LDFLAGS_save} ${LIBS_twolame}"
    AC_CHECK_HEADERS(twolame.h, ,
      [ AC_MSG_ERROR([Cannot find development header for libtwolame...]) ])
    AC_CHECK_LIB(twolame, twolame_init, [
      VLC_ADD_PLUGINS([twolame])
      VLC_ADD_CPPFLAGS([twolame],[-DLIBTWOLAME_STATIC])
      VLC_ADD_LIBS([twolame],[-ltwolame]) ],
        [ AC_MSG_ERROR([Cannot find libtwolame library...]) ])
    LDFLAGS="${LDFLAGS_save}"  
    CPPFLAGS="${CPPFLAGS_save}"
  fi
fi

dnl
dnl  QuickTime plugin
dnl
AC_ARG_ENABLE(quicktime,
  [  --enable-quicktime      QuickTime module (default enabled on MacOS X)])
if test "${enable_quicktime}" != "no" &&
  (test "${SYS}" = "darwin" || test "${enable_quicktime}" = "yes")
then
  if test "${SYS}" = "mingw32"; then
    VLC_ADD_BUILTINS([quicktime])
  else
  AC_CHECK_HEADERS(QuickTime/QuickTime.h,
    [ VLC_ADD_BUILTINS([quicktime])
      VLC_ADD_LDFLAGS([quicktime],[-Wl,-framework,QuickTime,-framework,Carbon])
    ], [ AC_MSG_ERROR([cannot find QuickTime headers]) ])
  fi
fi

dnl
dnl  Real plugin
dnl
AC_ARG_ENABLE(real,
  [  --enable-real           Real audio module (default disabled)])
if test "${enable_real}" = "yes"; then
  VLC_ADD_PLUGINS([realaudio])
fi

dnl
dnl  Real RTSP plugin
dnl
AC_ARG_ENABLE(realrtsp,
  [  --enable-realrtsp       Real RTSP module (default disabled)])
if test "${enable_realrtsp}" = "yes"; then
  VLC_ADD_PLUGINS([access_realrtsp])
fi

dnl
dnl MP4 module
dnl
AC_CHECK_HEADERS(zlib.h, [
  VLC_ADD_LIBS([mp4 skins2 sap mkv gme],[-lz])
] )

AC_CHECK_HEADERS(sysfs/libsysfs.h, [
  VLC_ADD_LIBS([mp4 mkv],[-lsysfs])
] )

dnl
dnl skins2 module
dnl
AC_ARG_ENABLE(libtar,
  [  --enable-libtar support for skins2 (default enabled)])

AS_IF([test "${enable_libtar}" != "no"],[
  AC_CHECK_HEADERS(libtar.h, [
    VLC_ADD_LIBS([skins2],[-ltar])
  ] )
])

dnl
dnl A52/AC3 decoder plugin
dnl
AC_ARG_ENABLE(a52,
  [  --enable-a52            A/52 support with liba52 (default enabled)])
if test "${enable_a52}" != "no"
then
  AC_ARG_WITH(a52,
    [    --with-a52=PATH       a52 headers and libraries])
  AC_ARG_WITH(a52-tree,
    [    --with-a52-tree=PATH  a52dec tree for static linking ],[],[])
  if test "${with_a52_tree}" != "no" -a -n "${with_a52_tree}"
  then
    real_a52_tree="`cd ${with_a52_tree} 2>/dev/null && pwd`"
    if test -z "${real_a52_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([${with_a52_tree} directory doesn't exist])
    fi
    dnl  Use a custom a52dec
    AC_MSG_CHECKING(for a52.h in ${real_a52_tree}/include)
    if test -f ${real_a52_tree}/include/a52.h
    then
      AC_MSG_RESULT(yes)
      VLC_ADD_CPPFLAGS([a52tofloat32],[-I${real_a52_tree}])
      VLC_ADD_LIBS([a52tofloat32],[-L${real_a52_tree}/liba52/.libs])
      LDFLAGS="${LDFLAGS_save} ${LIBS_a52tofloat32}"
      AC_CHECK_LIB(a52, a52_free, [
        VLC_ADD_BUILTINS([a52tofloat32])
        VLC_ADD_CPPFLAGS([a52tofloat32],[-DUSE_A52DEC_TREE])
        VLC_ADD_LIBS([a52tofloat32],[-la52])
        ],[
        if test -f ${real_a52_tree}/liba52/.libs/liba52.a
        then
          AC_MSG_ERROR([make sure you have at least a52dec-0.7.3 ($real_a52_tree)])
        else
          AC_MSG_ERROR([the specified tree hasn't been compiled])
        fi
      ])
      LDFLAGS="${LDFLAGS_save}"
    else
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([the specified tree doesn't have a52.h])
    fi
  else
    if test -z "${with_a52}"
    then
      LDFLAGS_test=""
      CPPFLAGS_test=""
    else
      LDFLAGS_test="-L${with_a52}/lib"
      CPPFLAGS_test="-I${with_a52}/include"
    fi
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_test} ${CPPFLAGS_a52tofloat32}"
    LDFLAGS="${LDFLAGS_save} ${LDFLAGS_test} ${LIBS_a52tofloat32}"
    AC_CHECK_HEADERS(a52dec/a52.h, [
      AC_CHECK_LIB(a52, a52_free, [
        VLC_ADD_PLUGINS([a52tofloat32])
        VLC_ADD_LIBS([a52tofloat32],[${LDFLAGS_test} -la52])
        VLC_ADD_CPPFLAGS([a52tofloat32],[${CPPFLAGS_test}])
      ],[
        AC_MSG_ERROR([Could not find liba52 on your system: you may get it from http://liba52.sf.net/. Alternatively you can use --disable-a52 to disable the a52 plugin.])
        ])
    ],[
      AC_MSG_ERROR([Could not find liba52 on your system: you may get it from http://liba52.sf.net/. Alternatively you can use --disable-a52 to disable the a52 plugin.])
    ])
    CPPFLAGS="${CPPFLAGS_save}"
    LDFLAGS="${LDFLAGS_save}"
  fi
fi

AC_ARG_WITH(a52-fixed,
      [    --with-a52-fixed      specify if liba52 has been compiled with fixed point support],
      [
        VLC_ADD_CPPFLAGS([a52tofloat32],[-DLIBA52_FIXED]) ])

dnl
dnl DTS Coherent Acoustics decoder plugin
dnl
AS_IF([test "x${enable_dts}" != "x"], [
  AC_MSG_ERROR([--enable-dts is obsolete. Please use libdca and --enable-dca.])
])
AS_IF([test "x${with_dts_tree}" != "x"], [
  AC_MSG_ERROR([--with-dts-tree is obsolete. Please use libdca and --with-dca-tree.])
])

AC_ARG_ENABLE(dca,
  [  --enable-dca            DTS Coherent Acoustics support with libdca (default enabled)])
AS_IF([test "${enable_dca}" != "no"], [
  AC_ARG_WITH(dca-tree,
    [    --with-dca-tree=PATH  libdca tree for static linking],,
    [with_dca_tree="no"])
  AS_IF([test "${with_dca_tree}" != "no"], [
    real_dca_tree="`cd ${with_dca_tree} 2>/dev/null && pwd`"
    AS_IF([test -z "${real_dca_tree}"], [
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([${with_dca_tree} directory doesn't exist])
    ])
    dnl  Use a custom libdca
    AC_MSG_CHECKING(for libdca in ${real_dca_tree})
    AS_IF([test -f "${real_dca_tree}/libdca/.libs/libdca.a"], [
      AC_MSG_RESULT(yes)
      VLC_ADD_LIBS([dtstofloat32],[${real_dca_tree}/libdca/.libs/libdca.a])
      AS_IF([test -f "${real_dca_tree}/include/dca.h"], [
        VLC_ADD_CPPFLAGS([dtstofloat32],[-I${real_dca_tree}/include])
      ], [
        VLC_ADD_CPPFLAGS([dtstofloat32],[-I${real_dca_tree}/../include])
      ])
    ], [
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([the specified tree doesn't have libdca/.libs/libdca.a])
    ])
  ], [
    PKG_CHECK_MODULES([DCA], [libdca >= 0.0.5], [
      VLC_ADD_PLUGINS([dtstofloat32])
      VLC_ADD_CPPFLAGS([dtstofloat32], [${DCA_CFLAGS}])
      VLC_ADD_LIBS([dtstofloat32], [${DCA_LIBS}])
    ], [
      AS_IF([test "x${enable_dca}" != "x"], [
        AC_MSG_ERROR([${DCA_PKG_ERRORS}])
      ])
    ])
  ])
])

dnl
dnl  Flac plugin
dnl
AC_ARG_ENABLE(flac,
  [  --enable-flac           libflac decoder/encoder support (default disabled)])
if test "${enable_flac}" = "yes"
then
  AC_ARG_WITH(flac-tree,
  [    --with-flac-tree=PATH flac tree for static linking])
  if test -n "${with_flac_tree}"
  then
    AC_MSG_CHECKING(for libFLAC.a in ${with_flac_tree})
    real_flac_tree="`cd ${with_flac_tree} 2>/dev/null && pwd`"
    if test -z "${real_flac_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_flac_tree}])
    fi
    if test -f "${real_flac_tree}/src/libFLAC/.libs/libFLAC.a"
    then
      dnl  Use a custom flac
      AC_MSG_RESULT(${real_flac_tree}/src/libFLAC/.libs/libFLAC.a)
      VLC_ADD_LIBS([flac],[${real_flac_tree}/src/libFLAC/.libs/libFLAC.a])
      VLC_ADD_CFLAGS([flac],[-I${real_flac_tree}/include])
      AC_DEFINE(HAVE_FLAC_STREAM_DECODER_H, 1, [Define if you have FLAC])
    else
      dnl  The given flac wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_flac_tree}/src/libFLAC/.libs/libFLAC.a, make sure you compiled flac in ${with_flac_tree}])
    fi
  else
    AC_CHECK_HEADERS(FLAC/stream_decoder.h, [
      VLC_ADD_LIBS([flac],[-lFLAC -logg])
     ],[])
  fi
fi

dnl
dnl  Libmpeg2 plugin
dnl
AC_ARG_ENABLE(libmpeg2,
  [  --enable-libmpeg2       libmpeg2 decoder support (default enabled)])
if test "${enable_libmpeg2}" != "no"
then
  AC_ARG_WITH(libmpeg2-tree,
  [    --with-libmpeg2-tree=PATH libmpeg2 tree for static linking])
  if test -n "${with_libmpeg2_tree}"
  then
    AC_MSG_CHECKING(for libmpeg2.a in ${with_libmpeg2_tree})
    real_libmpeg2_tree="`cd ${with_libmpeg2_tree} 2>/dev/null && pwd`"
    if test -z "${real_libmpeg2_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_libmpeg2_tree}])
    fi
    if test -f "${real_libmpeg2_tree}/libmpeg2/.libs/libmpeg2.a"
    then
      dnl  Use a custom libmpeg2
      AC_MSG_RESULT(${real_libmpeg2_tree}/libmpeg2/.libs/libmpeg2.a)
      VLC_ADD_BUILTINS([libmpeg2])
      VLC_ADD_LIBS([libmpeg2],[-L${real_libmpeg2_tree}/libmpeg2/.libs -lmpeg2])
      VLC_ADD_CFLAGS([libmpeg2],[-I${real_libmpeg2_tree}/include])
      eval "`cd ${real_libmpeg2_tree}/include && ln -sf . mpeg2dec 2>/dev/null`"
    else
      dnl  The given libmpeg2 wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_libmpeg2_tree}/libmpeg2/.libs/libmpeg2.a, make sure you compiled libmpeg2 in ${with_libmpeg2_tree}])
    fi
  else
    AC_CHECK_HEADERS(mpeg2dec/mpeg2.h, [
      AC_MSG_CHECKING(for libmpeg2 version >= 0.3.2)
      AC_EGREP_CPP(yes,
        [#include <mpeg2dec/mpeg2.h>
         #ifdef MPEG2_RELEASE
         #if MPEG2_RELEASE >= MPEG2_VERSION(0,3,2)
         yes
         #endif
         #endif],
        [AC_MSG_RESULT([yes])
          VLC_ADD_PLUGINS([libmpeg2])
          VLC_ADD_LIBS([libmpeg2],[-lmpeg2])],
        [AC_MSG_RESULT([no])
          AC_MSG_ERROR([Your libmpeg2 is too old (you need the cvs version): you may get a more recent one from http://libmpeg2.sf.net/. Alternatively you can use --disable-libmpeg2 to disable the libmpeg2 plugin.])])],

      [AC_MSG_ERROR([Could not find libmpeg2 on your system: you may get it from http://libmpeg2.sf.net/ (you need the cvs version). Alternatively you can use --disable-libmpeg2 to disable the libmpeg2 plugin.])]
    )
  fi
fi

dnl
dnl  Vorbis plugin
dnl
AC_ARG_ENABLE(vorbis,
  [  --enable-vorbis         Vorbis decoder support (default enabled)])
if test "${enable_vorbis}" != "no"
then
  AC_ARG_WITH(vorbis-tree,
  [    --with-vorbis-tree=PATH vorbis tree for static linking])
  if test -n "${with_vorbis_tree}"
  then
    AC_MSG_CHECKING(for libvorbis.a in ${with_vorbis_tree})
    real_vorbis_tree="`cd ${with_vorbis_tree} 2>/dev/null && pwd`"
    if test -z "${real_vorbis_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_vorbis_tree}])
    fi
    if test -f "${real_vorbis_tree}/lib/.libs/libvorbis.a"
    then
      dnl  Use a custom vorbis 
      AC_MSG_RESULT(${real_vorbis_tree}/lib/.libs/libvorbis.a)
      VLC_ADD_PLUGINS([vorbis])
      VLC_ADD_LIBS([vorbis],[${real_vorbis_tree}/lib/.libs/libvorbis.a ${real_vorbis_tree}/lib/.libs/libvorbisenc.a])
      VLC_ADD_CFLAGS([vorbis],[-I${real_vorbis_tree}/include])
    else
      dnl  The given vorbis wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_vorbis_tree}/lib/.libs/libvorbis.a, make sure you compiled vorbis in ${with_vorbis_tree}])
    fi
  else
    AC_CHECK_HEADERS(vorbis/codec.h, [
      VLC_ADD_PLUGINS([vorbis])
      VLC_ADD_LIBS([vorbis],[-lvorbis -logg -lm]) ],[])

    AC_CHECK_HEADERS(vorbis/vorbisenc.h, [
      VLC_ADD_LIBS([vorbis],[-lvorbisenc -lm]) ],[])
  fi
fi

dnl
dnl  Tremor plugin
dnl
AC_ARG_ENABLE(tremor,
  [  --enable-tremor         Tremor decoder support (default disabled)])
if test "${enable_tremor}" = "yes"
then
  AC_CHECK_HEADERS(tremor/ivorbiscodec.h, [
    VLC_ADD_PLUGINS([tremor])
    VLC_ADD_LIBS([tremor],[-lvorbisidec -logg])
   ],[])
fi

dnl
dnl  Speex plugin
dnl
AC_ARG_ENABLE(speex,
  [  --enable-speex          Speex decoder support (default enabled)])
if test "${enable_speex}" != "no"
then
  AC_ARG_WITH(speex-tree,
  [    --with-speex-tree=PATH speex tree for static linking])
  if test -n "${with_speex_tree}"
  then
    AC_MSG_CHECKING(for libspeex.a in ${with_speex_tree})
    real_speex_tree="`cd ${with_speex_tree} 2>/dev/null && pwd`"
    if test -z "${real_speex_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_speex_tree}])
    fi
    if test -f "${real_speex_tree}/libspeex/.libs/libspeex.a"
    then
      dnl  Use a custom speex
      AC_MSG_RESULT(${real_speex_tree}/libspeex/.libs/libspeex.a)
      VLC_ADD_PLUGINS([speex])
      VLC_ADD_LIBS([speex],[${real_speex_tree}/libspeex/.libs/libspeex.a])
      VLC_ADD_CFLAGS([speex],[-I${real_speex_tree}/include])
    else
      dnl  The given speex wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_speex_tree}/libspeex/.libs/libspeex.a, make sure you compiled speex in ${with_speex_tree}])
    fi
  else
    AC_CHECK_HEADERS(speex/speex.h, [
      LDFLAGS="${LDFLAGS_save} ${LIBS_speex}"
      AC_CHECK_LIB(speex, speex_decode_int, [
        VLC_ADD_PLUGINS([speex])
        VLC_ADD_LIBS([speex],[-lspeex]) ],
        [ AC_MSG_RESULT([no])
          AC_MSG_WARN([Your libspeex is too old, please get the development
                       version.]) ],[])
      LDFLAGS="${LDFLAGS_save}"
      ],[])
  fi
fi

dnl
dnl  tarkin decoder plugin
dnl
AC_ARG_ENABLE(tarkin,
[  --enable-tarkin         experimental tarkin codec (default disabled)])
if test "${enable_tarkin}" = "yes"
then
  AC_ARG_WITH(tarkin-tree,
  [    --with-tarkin-tree=PATH tarkin tree for static linking])
  if test -n "${with_tarkin_tree}"
  then
    AC_MSG_CHECKING(for tarkin.o in ${with_tarkin_tree})
    real_tarkin_tree="`cd ${with_tarkin_tree} 2>/dev/null && pwd`"
    if test -f "${real_tarkin_tree}/tarkin.o"
    then
      VLC_ADD_BUILTINS([tarkin])
      VLC_ADD_CPPFLAGS([tarkin],[-I${real_tarkin_tree}])
      VLC_ADD_LIBS([tarkin],[${real_tarkin_tree}/mem.o ${real_tarkin_tree}/pnm.o ${real_tarkin_tree}/wavelet.o ${real_tarkin_tree}/wavelet_xform.o ${real_tarkin_tree}/wavelet_coeff.o ${real_tarkin_tree}/yuv.o ${real_tarkin_tree}/tarkin.o ${real_tarkin_tree}/info.o -logg])
      AC_MSG_RESULT(yes)
    else
      dnl  The given tarkin tree wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_tarkin_tree}/tarkin.o,
                    make sure you compiled tarkin in ${with_tarkin_tree}])
    fi
  fi
fi

dnl
dnl  theora decoder plugin
dnl
AC_ARG_ENABLE(theora,
[  --enable-theora         experimental theora codec (default disabled)])
if test "${enable_theora}" = "yes"
then
  AC_CHECK_HEADERS(theora/theora.h, [
    AC_CHECK_LIB(theora, theora_granule_time, [
      VLC_ADD_PLUGINS([theora])
      theora_libs="-ltheora -logg"
      VLC_ADD_LIBS([theora],[${theora_libs}]) ],[
      AC_MSG_ERROR([libtheora doesn't appear to be installed on your system.
You also need to check that you have a libogg posterior to the 1.0 release.])],
      [-logg])
  ])
fi

dnl
dnl  dirac decoder plugin
dnl
AC_ARG_ENABLE(dirac,
[  --enable-dirac          experimental dirac codec (default disabled)])
if test "${enable_dirac}" = "yes"; then
  PKG_CHECK_MODULES(DIRAC,[dirac >= 0.9.0], [
      VLC_ADD_PLUGINS([dirac])
      VLC_ADD_CFLAGS([dirac],[$DIRAC_CFLAGS])
      VLC_ADD_LIBS([dirac],[$DIRAC_LIBS -lstdc++]) ],[
      AC_MSG_ERROR([libdirac doesn't appear to be installed on you system.])
  ])
fi

dnl
dnl  PNG decoder module
dnl
AC_ARG_ENABLE(png,
  [  --enable-png            PNG support (default enabled)])
if test "${enable_png}" != "no"; then
AC_CHECK_HEADERS(png.h, [
  LDFLAGS="${LDFLAGS_save} -lz"
  AC_CHECK_LIB(png, png_set_rows, [
    VLC_ADD_LIBS([png],[-lpng -lz])
    VLC_ADD_PLUGINS([png])
    VLC_ADD_PLUGINS([osdmenu osd_parser])
    AC_DEFINE(HAVE_LIBPNG, 1, [Define if you have the PNG library: libpng])],
    [],[-lz])
  LDFLAGS="${LDFLAGS_save}"
  ])
fi

dnl
dnl H264 encoder plugin (using libx264)
dnl
AC_ARG_ENABLE(x264,
  [  --enable-x264           H264 encoding support with libx264 (default enabled)])
if test "${enable_x264}" != "no"; then
  AC_ARG_WITH(x264-tree,
    [    --with-x264-tree=PATH x264 tree for static linking ],[],[])
  if test "${with_x264_tree}" != "no" -a -n "${with_x264_tree}"
  then
    real_x264_tree="`cd ${with_x264_tree} 2>/dev/null && pwd`"
    if test -z "${real_x264_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([${with_x264_tree} directory doesn't exist])
    fi
    dnl  Use a custom libx264
    AC_MSG_CHECKING(for x264.h in ${real_x264_tree})
    if test -f ${real_x264_tree}/x264.h
    then
      AC_MSG_RESULT(yes)
      VLC_ADD_CPPFLAGS([x264],[-I${real_x264_tree}])
      VLC_ADD_LIBS([x264],[-L${real_x264_tree}])
      PKG_CHECK_MODULES(X264,x264, [
        VLC_ADD_PLUGINS([x264])
        VLC_ADD_LDFLAGS([x264],[${X264_LIBS}])
        VLC_ADD_CFLAGS([x264],[${X264_CFLAGS}])
      ],[
        AC_MSG_ERROR([the specified tree hasn't been compiled])
      ])
      LDFLAGS="${LDFLAGS_save}"
    else
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([the specified tree doesn't have x264.h])
    fi
  else
    AC_CHECK_HEADERS(x264.h, [
      PKG_CHECK_MODULES(X264,x264, [
        VLC_ADD_PLUGINS([x264])
        VLC_ADD_LDFLAGS([x264],[${X264_LIBS}])
        VLC_ADD_CFLAGS([x264],[${X264_CFLAGS}])
      ],[
        if test "${enable_x264}" = "yes"; then
            AC_MSG_ERROR([Could not find libx264 on your system: you may get it from http://www.videolan.org/x264.html])
          fi
      ])
    ])
    LDFLAGS="${LDFLAGS_save}"
  fi
fi

dnl
dnl libfluidsynth (MIDI synthetizer) plugin
dnl
AC_ARG_ENABLE(fluidsynth,
  [  --enable-fluidsynth     MIDI synthesisr with libfluidsynth (default enabled)])
AS_IF([test "x${enable_fluidsynth}" != "xno"], [
  PKG_CHECK_MODULES(FLUIDSYNTH, fluidsynth, [
    VLC_ADD_PLUGINS(fluidsynth)
    VLC_ADD_CFLAGS(fluidsynth, [${FLUIDSYNTH_CFLAGS}])
    VLC_ADD_LIBS(fluidsynth, [${FLUIDSYNTH_LIBS}])
  ], [
    AS_IF([test "x${enable_fluidsynth}" != "x"], [
      AC_MSG_ERROR([${FLUIDSYNTH_PKG_ERRORS}])
    ])
  ])
])

dnl
dnl Teletext Modules
dnl vbi decoder plugin (using libzbvi)
dnl telx module
dnl uncompatible
dnl
AC_ARG_ENABLE(zvbi,
  [  --enable-zvbi           VBI (inc. Teletext) decoding support with libzvbi (default enabled)])
AC_ARG_ENABLE(telx,
  [  --enable-telx,          Teletext decoding module (conflicting with zvbi) (default disabled)])

AS_IF( [test "${enable_zvbi}" != "no"],[
  AS_IF( [test "${enable_telx}" = "yes"],[
    AC_MSG_ERROR([The zvbi and telx modules are uncompatibles. Disable the other if you enable one.])
  ],[
    PKG_CHECK_MODULES(ZVBI,
    	zvbi-0.2 >= 0.2.25,
    	[
    	  VLC_ADD_LIBS([zvbi],[$ZVBI_LIBS])
    	  VLC_ADD_CFLAGS([zvbi],[$ZVBI_CFLAGS])
    	  VLC_ADD_PLUGINS([zvbi])
	  AC_DEFINE(ZVBI_COMPILED, 1, [Define if the zvbi module is built]) 
	],[
	  AC_MSG_WARN(ZVBI library not found. Enabling the telx module instead)
	  enable_telx="yes"
	])
    ])  
  ])
AS_IF( [test "${enable_telx}" = "yes"],[
  VLC_ADD_PLUGINS([telx])
  ])

dnl
dnl asa/csri subtitle rendering module
dnl
AC_ARG_ENABLE(csri,
  [  --enable-csri          Subtitle support using CSRI / asa (default disabled)])
AS_IF( [test "${enable_csri}" = "yes"], [
  PKG_CHECK_MODULES(CSRI,
      csri >= 0.1.0,
      [
        VLC_ADD_LDFLAGS([csri],[$CSRI_LIBS])
        VLC_ADD_CFLAGS([csri],[$CSRI_CFLAGS])
        VLC_ADD_PLUGINS([csri])
      ],[
        AC_MSG_WARN([CSRI helper library not found])
      ])
  ])

dnl
dnl asa demuxer
dnl
AC_ARG_ENABLE(asademux,
  [  --enable-asademux      asa subtitle demuxing (default disabled)])
AS_IF( [test "${enable_asademux}" = "yes"], [
  PKG_CHECK_MODULES(PCRE,
      libpcre >= 6.5,
      [
        VLC_ADD_LDFLAGS([asademux],[$PCRE_LIBS])
        VLC_ADD_CFLAGS([asademux],[$PCRE_CFLAGS])
        VLC_ADD_PLUGINS([asademux])
      ],[
        AC_MSG_WARN([PCRE library not found (required for asademux)])
      ])
  ])

dnl
dnl  CMML plugin
dnl
AC_ARG_ENABLE(cmml,
  [  --enable-cmml           CMML support (default enabled)])
if test "${enable_cmml}" != "no"
then
  VLC_ADD_PLUGINS([cmml])
fi


dnl
dnl  Video plugins
dnl

AC_ARG_WITH(,[Video plugins:])

dnl Check for DPMS
if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
  CPPFLAGS="${CPPFLAGS_save} ${X_CFLAGS}"
  AC_CHECK_HEADERS(X11/extensions/dpms.h, [
    AC_MSG_CHECKING(for DPMSInfo in X11/extensions/dpms.h)
    AC_EGREP_HEADER(DPMSInfo,X11/extensions/dpms.h,[
      AC_MSG_RESULT(yes)
      AC_DEFINE(DPMSINFO_IN_DPMS_H, 1,
                Define if <X11/extensions/dpms.h> defines DPMSInfo.)
    ],[
      AC_MSG_RESULT(no)
    ])
  ],,[
    #include <X11/Xlib.h>
  ])
  CPPFLAGS="${CPPFLAGS_save}"
fi

dnl
dnl  X11 module
dnl  (enabled by default except on win32)
dnl
AC_ARG_ENABLE(x11,
  [  --enable-x11            X11 support (default enabled)])
if test "${enable_x11}" != "no" &&
  (test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" ||
   test "${enable_x11}" = "yes"); then
  CPPFLAGS="${CPPFLAGS_save} ${X_FLAGS}"
  AC_CHECK_HEADERS(X11/Xlib.h, [
    VLC_ADD_PLUGINS([panoramix])
    VLC_ADD_LIBS([panoramix],[${X_LIBS} ${X_PRE_LIBS} -lX11])
    VLC_ADD_CPPFLAGS([panoramix],[${X_CFLAGS}])
    AC_CHECK_LIB(Xext, XShmAttach, [
      VLC_ADD_PLUGINS([x11])
      VLC_ADD_LIBS([x11],[${X_LIBS} ${X_PRE_LIBS} -lX11 -lXext])
      VLC_ADD_CPPFLAGS([x11],[${X_CFLAGS}])
    ])
  ])
  CPPFLAGS="${CPPFLAGS_save}"
fi

dnl
dnl  XVideo module
dnl  (enabled by default except on win32)
dnl
AC_ARG_ENABLE(xvideo,
  [  --enable-xvideo         XVideo support (default enabled)])
if test "${enable_xvideo}" != "no" &&
  (test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" ||
   test "${enable_xvideo}" = "yes"); then
  CPPFLAGS="${CPPFLAGS_save} ${X_CFLAGS}"
  AC_CHECK_HEADERS(X11/extensions/Xv.h, [
    CFLAGS="${CFLAGS_save} ${X_LIBS} ${X_PRE_LIBS} -lX11 -lXext"
    AC_CHECK_LIB(Xv,XvPutImage,[
      # If libXv.so is available, xvideo can be a plugin. Otherwise, we
      # test for libXv_pic.
      if test -f /usr/X11R6/lib/libXv.so -o -f /usr/lib/libXv.so -o -f "${x_libraries}"/libXv.so; then
        VLC_ADD_PLUGINS([xvideo])
        VLC_ADD_CPPFLAGS([xvideo],[${X_CFLAGS}])
        VLC_ADD_LIBS([xvideo],[${X_LIBS} ${X_PRE_LIBS} -lX11 -lXext -lXv])
      else
        AC_CHECK_LIB(Xv_pic,XvPutImage,[
          VLC_ADD_PLUGINS([xvideo])
          VLC_ADD_CPPFLAGS([xvideo],[${X_CFLAGS}])
          VLC_ADD_LIBS([xvideo],[${X_LIBS} ${X_PRE_LIBS} -lX11 -lXext -lXv_pic])
        ],[
          VLC_ADD_BUILTINS([xvideo])
          VLC_ADD_LIBS([xvideo],[${X_LIBS} ${X_PRE_LIBS} -lX11 -lXext -lXv])
          VLC_ADD_CPPFLAGS([xvideo],[${X_CFLAGS}])
        ])
      fi
    ])
    CFLAGS="${CFLAGS_save}"
  ]
  CPPFLAGS="${CPPFLAGS_save}")
fi

dnl
dnl  GLX module
dnl  (enabled by default except on win32)
dnl
AC_ARG_ENABLE(glx,
  [  --enable-glx            X11 OpenGL (GLX) support (default enabled)])
if test "${enable_glx}" != "no" &&
  (test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" ||
   test "${enable_glx}" = "yes"); then
  CPPFLAGS="${CPPFLAGS_save} ${X_CFLAGS}"
  AC_CHECK_HEADERS(X11/Xlib.h GL/glu.h GL/glx.h)
  AC_COMPILE_IFELSE(AC_LANG_PROGRAM(
    [[#if !defined(HAVE_X11_XLIB_H) || !defined(HAVE_GL_GLU_H) || !defined(HAVE_GL_GLX_H)
    choke me
    #endif]]),
    [
      VLC_ADD_PLUGINS([glx])
      VLC_ADD_LIBS([glx],[${X_LIBS} ${X_PRE_LIBS} -lX11 -lXext -lGL -lGLU])
      VLC_ADD_CPPFLAGS([glx],[${X_CFLAGS}])
    ],[AC_MSG_ERROR([Please install GL development package. Alternatively you can also configure with --disable-glx.])])
  CPPFLAGS="${CPPFLAGS_save}"
fi

dnl
dnl  XVMC module
dnl  (enabled by default except on win32)
dnl
AC_ARG_ENABLE(xvmc,
  [  --enable-xvmc           XVMC support (default disabled)])
if test "${enable_xvmc}" = "yes" &&
  (test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" ||
   test "${enable_xvmc}" = "yes"); then
  CPPFLAGS="${CPPFLAGS_save} ${X_FLAGS}"
  AC_CHECK_HEADERS(X11/extensions/vldXvMC.h, [
    VLC_ADD_PLUGINS([xvmc])
    VLC_ADD_LIBS([xvmc],[${X_LIBS} ${X_PRE_LIBS} -lX11 -lXext])
    VLC_ADD_CPPFLAGS([xvmc],[${X_CFLAGS}])
  ])
  CPPFLAGS="${CPPFLAGS_save}"
fi

dnl
dnl  Check for the Xinerama extension
dnl
AC_ARG_ENABLE(xinerama,
  [  --enable-xinerama       Xinerama support (default enabled)])
if test "${enable_xvideo}" != "no" && test "${enable_xinerama}" != "no" &&
  (test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" ||
   test "${enable_xvideo}" = "yes"); then
  ac_cv_have_xinerama="no"
  CPPFLAGS="${CPPFLAGS_save} ${X_CFLAGS}"
  CFLAGS="${CFLAGS_save} ${X_LIBS} ${X_PRE_LIBS} -lX11 -lXext"
  AC_CHECK_HEADERS(X11/extensions/Xinerama.h,[
    AC_CHECK_LIB(Xinerama_pic, XineramaQueryExtension,[
      VLC_ADD_LIBS([xvideo],[-lXinerama_pic])
      VLC_ADD_LIBS([x11],[-lXinerama_pic])
      VLC_ADD_LIBS([glx],[-lXinerama_pic])
      ac_cv_have_xinerama="yes"
    ],[
      AC_CHECK_LIB(Xinerama, XineramaQueryExtension,[
        VLC_ADD_LIBS([xvideo],[-lXinerama])
        VLC_ADD_LIBS([x11],[-lXinerama])
        VLC_ADD_LIBS([glx],[-lXinerama])
        ac_cv_have_xinerama="yes"
      ])
    ])
  ])
  if test "${ac_cv_have_xinerama}" = "yes"; then
    AC_DEFINE(HAVE_XINERAMA, 1, [Define this if you have libXinerama installed])
  fi
  CFLAGS="${CFLAGS_save}"
  CPPFLAGS="${CPPFLAGS_save}"

dnl
dnl  Check for XF86VidMode extension
dnl
  ac_cv_have_xf86vidmode="no"
  CPPFLAGS="${CPPFLAGS_save} ${X_CFLAGS}"
  CFLAGS="${CFLAGS_save} ${X_LIBS} ${X_PRE_LIBS} -lX11 -lXext"
  AC_CHECK_HEADERS(X11/extensions/xf86vmode.h,[
    AC_CHECK_LIB(Xxf86vm_pic, XF86VidModeGetViewPort,[
      VLC_ADD_LIBS([xvideo],[-lXxf86vm_pic])
      VLC_ADD_LIBS([x11],[-lXxf86vm_pic])
      VLC_ADD_LIBS([glx],[-lXxf86vm_pic])
      ac_cv_have_xf86vidmode="yes"
    ],[
      AC_CHECK_LIB(Xxf86vm, XF86VidModeGetViewPort,[
        VLC_ADD_LIBS([xvideo],[-lXxf86vm])
        VLC_ADD_LIBS([x11],[-lXxf86vm])
        VLC_ADD_LIBS([glx],[-lXxf86vm])
        ac_cv_have_xf86vidmode="yes"
      ])
    ])
  ],[true],
[#ifdef HAVE_X11_XLIB_H
# include <X11/Xlib.h>
#endif]
   )
  AS_IF([test "${ac_cv_have_xf86vidmode}" = "yes"],
    [AC_DEFINE(HAVE_XF86VIDMODE, 1, [Define this if you have libXxf86vm installed])
  ])
  CFLAGS="${CFLAGS_save}"
  CPPFLAGS="${CPPFLAGS_save}"

fi

dnl
dnl  OpenGL module
dnl  (enabled by default except on beos)
dnl
AC_ARG_ENABLE(opengl,
  [  --enable-opengl         OpenGL support (default enabled)])
if test "${enable_opengl}" != "no" &&
   test "${SYS}" != "beos" -a "${SYS}" != "mingwce"; then
  if test "${SYS}" != "darwin"; then
    AC_CHECK_HEADERS(GL/gl.h GL/glu.h, [
      VLC_ADD_PLUGINS([opengl])
      if test "${SYS}" != "mingw32"; then
        VLC_ADD_LIBS([opengl],[${X_LIBS} -lGL -lGLU])
      else
        VLC_ADD_LIBS([opengl],[-lopengl32 -lglu32])
      fi
    ])
  else
    dnl OS X special case (no GL/gl.h but OpenGL/gl.h)
    VLC_ADD_PLUGINS([opengl])
    VLC_ADD_LIBS([opengl],[-Wl,-framework,OpenGL])
  fi
fi

dnl
dnl  SDL module
dnl
AC_ARG_ENABLE(sdl,
  [  --enable-sdl            SDL support (default enabled)])
AC_ARG_ENABLE(sdl-image,
  [  --enable-sdl-image      SDL image support (default enabled)])
if test "${enable_sdl}" != "no"
then
  SDL_PATH="${PATH}"
  AC_ARG_WITH(sdl-config-path,
    [    --with-sdl-config-path=PATH sdl-config path (default search in \$PATH)],
    [ if test "${with_sdl_config_path}" != "no"
      then
        SDL_PATH="${with_sdl_config_path}:${PATH}"
      fi ])
  AC_PATH_PROG(SDL12_CONFIG, sdl12-config, no, ${SDL_PATH})
  SDL_CONFIG="${SDL12_CONFIG}"
  SDL_HEADER="SDL12/SDL.h"
  SDL_IMAGE="SDL12/SDL_image.h"
  if test "${SDL_CONFIG}" = "no"
  then
    AC_PATH_PROG(SDL11_CONFIG, sdl11-config, no, ${SDL_PATH})
    SDL_CONFIG=${SDL11_CONFIG}
    SDL_HEADER="SDL11/SDL.h"
    SDL_IMAGE="SDL11/SDL_image.h"
  fi
  if test "${SDL_CONFIG}" = "no"
  then
    AC_PATH_PROG(SDL_CONFIG, sdl-config, no, ${SDL_PATH})
    SDL_HEADER="SDL/SDL.h"
    SDL_IMAGE="SDL/SDL_image.h"
  fi
  # check for cross-compiling
  SDL_PREFIX=
  AC_ARG_WITH(sdl-prefix,
    [    --with-sdl-prefix=PATH path to libsdl (needed for cross-compiling),
                               e.g use as:
                               --with-sdl-prefix=/usr/local/arm/2.95.3/arm-linux/usr)],[],[])
  if test "${with_sdl_prefix}" != "no" -a -n "${with_sdl_prefix}"
  then
    SDL_PREFIX="--prefix=${with_sdl_prefix}"
  fi
  if test "${SDL_CONFIG}" != "no"
  then
    # SDL on Darwin is heavily patched and can only run SDL_image
    if test "${SYS}" != "darwin" -a "${SYS}" != "mingw32"; then
      VLC_ADD_PLUGINS([vout_sdl aout_sdl])
    fi
    VLC_ADD_CFLAGS([vout_sdl aout_sdl sdl_image],[`${SDL_CONFIG} ${SDL_PREFIX} --cflags | sed 's,SDL,,'`])
    VLC_ADD_LIBS([vout_sdl aout_sdl sdl_image],[`${SDL_CONFIG} ${SDL_PREFIX} --libs | sed 's,-rdynamic,,'`])
    CPPFLAGS="${CPPFLAGS_save} ${CFLAGS_vout_sdl}"
    AC_CHECK_HEADERS(${SDL_HEADER}, AC_DEFINE_UNQUOTED(SDL_INCLUDE_FILE,
      <${SDL_HEADER}>, Indicate whether we should use SDL/SDL.h or SDL11/SDL.h),
      [ AC_CHECK_HEADERS(SDL.h, AC_DEFINE(SDL_INCLUDE_FILE, <SDL.h>,
          As a last resort we also test for SDL.h presence),
      [ AC_MSG_ERROR([The development package for SDL is not installed.
Please install it and try again. Alternatively you can also configure with
--disable-sdl.])
      ])])
    AS_IF([ test "${enable_sdl_image}" != "no"],[
      AC_CHECK_HEADERS(${SDL_IMAGE}, [AC_DEFINE_UNQUOTED(SDL_IMAGE_INCLUDE_FILE,
        <${SDL_IMAGE}>, Indicate the path of SDL_image.h)
        VLC_ADD_PLUGINS([sdl_image])
        AC_CHECK_LIB(png, png_set_rows,
          [VLC_ADD_LIBS([sdl_image],[-lpng -lz])],[],[-lz])
        AC_CHECK_LIB(jpeg, jpeg_start_decompress,
          [VLC_ADD_LIBS([sdl_image],[-ljpeg])])
        AC_CHECK_LIB(tiff, TIFFClientOpen,
          [VLC_ADD_LIBS([sdl_image],[-ltiff])])
        VLC_ADD_LIBS([sdl_image], [-lSDL_image])],
        [ AC_CHECK_HEADERS(SDL_image.h, AC_DEFINE(SDL_IMAGE_INCLUDE_FILE, <SDL_image.h>,
            As a last resort we also test for SDL_image.h presence),
        [ AC_MSG_WARN([The development package for SDL_image is not installed.
  You should install it alongside your SDL package.])
        ])])
    ])
    CPPFLAGS="${CPPFLAGS_save}"
    if expr 1.1.5 \> `${SDL_CONFIG} --version` >/dev/null
    then
      AC_MSG_ERROR([The development package for SDL is not installed.
Please install it and try again. Alternatively you can also configure with
--disable-sdl.])
    fi

  elif test "${enable_sdl}" =  "yes"
  then
    AC_MSG_ERROR([I couldn't find the SDL package. You can download libSDL
from http://www.libsdl.org/, or configure with --disable-sdl. Have a nice day.
    ])
  fi
fi

dnl
dnl  freetype module
dnl
AC_ARG_ENABLE(freetype,
  [  --enable-freetype       freetype support (default enabled)])
AC_ARG_ENABLE(fribidi,
  [  --enable-fribidi        fribidi support (default enabled)])
if test "${enable_freetype}" != "no"
then
   PKG_CHECK_MODULES(FREETYPE, freetype2,[  
      VLC_ADD_PLUGINS([freetype])
      have_freetype=yes     
      VLC_ADD_CPPFLAGS([freetype skins2],[${FREETYPE_CFLAGS}])
      VLC_ADD_LIBS([freetype skins2],[${FREETYPE_LIBS}])
      if test "${SYS}" = "mingw32"; then
        VLC_ADD_LIBS([freetype skins2],[-lxml2])
     fi
    AC_CHECK_HEADERS(fontconfig/fontconfig.h,
      [VLC_ADD_CPPFLAGS([freetype],[-DHAVE_FONTCONFIG])
       VLC_ADD_LIBS([freetype],[-lfontconfig])])
    AC_CHECK_HEADERS(Carbon/Carbon.h,
      [VLC_ADD_LDFLAGS([freetype],[-Wl,-framework,Carbon])])
  ],[
  have_freetype=no
  AS_IF([ test "${enable_freetype}" =  "yes"],[
    AC_MSG_ERROR([I couldn't find the freetype package. You can download libfreetype2
from http://www.freetype.org/, or configure with --disable-freetype. Have a nice day.
      ])
    ])
  ])

  dnl fribidi support
  if test "${enable_fribidi}" != "no"
  then
    PKG_CHECK_MODULES(FRIBIDI, fribidi, [
      VLC_ADD_CPPFLAGS([freetype skins2], [${FRIBIDI_CFLAGS} -DHAVE_FRIBIDI])
      VLC_ADD_LIBS([freetype skins2], [${FRIBIDI_LIBS}])
    ])
    fi
fi

dnl
dnl  libxml2 module
dnl
AC_ARG_ENABLE(libxml2,
  [  --enable-libxml2        libxml2 support (default enabled)])
if test "${enable_libxml2}" != "no"
then
  XML2_PATH="${PATH}"
  AC_ARG_WITH(xml2-config-path,
    [    --with-xml2-config-path=PATH xml2-config path (default search in \$PATH)],
    [ if test "${with_xml2_config_path}" != "no"; then
        XML2_PATH="${with_xml2_config_path}:${PATH}"
      fi ])
  AC_PATH_PROG(XML2_CONFIG, xml2-config, no, ${XML2_PATH})
  if test "${XML2_CONFIG}" != "no"; then
    VLC_ADD_CPPFLAGS([xml],[`${XML2_CONFIG} --cflags`])
    VLC_ADD_LIBS([xml],[`${XML2_CONFIG} --libs`])
    dnl depends on the xmlTextReader extension
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_xml}"
    LDFLAGS="${LDFLAGS_save} ${LIBS_xml}"
    AC_CHECK_LIB(xml2,xmlTextReaderConstName,[
      AC_EGREP_HEADER(xmlTextReaderConstName,libxml/xmlreader.h,[
        VLC_ADD_PLUGINS([xml]) ],[
          AC_MSG_WARN([libxml2 missing the xmlTextReader extension, you should update your version])
          if test "${enable_xml2}" = "yes"; then
            AC_MSG_ERROR([libxml2 missing the xmlTextReader extension])
          fi])
       ],[
      AC_MSG_WARN([libxml2 missing the xmlTextReader extension, you should update your version])
      if test "${enable_xml2}" = "yes"; then
        AC_MSG_ERROR([libxml2 missing the xmlTextReader extension])
      fi])
    LDFLAGS="${LDFLAGS_save}"
    CPPFLAGS="${CPPFLAGS_save}"
  else
    if test "${enable_xml2}" = "yes"; then
      AC_MSG_ERROR([Could not find libxml2])
    fi
  fi
fi

dnl
dnl  SVG module
dnl
AC_ARG_ENABLE(svg,
  [  --enable-svg            SVG support (default disabled)])
if test "${enable_svg}" = "yes"
then
  PKG_CHECK_MODULES(SVG, 
	librsvg-2.0 >= 2.9.0,
	[
 	  VLC_ADD_LIBS([svg],[$SVG_LIBS])
      	  VLC_ADD_CFLAGS([svg],[$SVG_CFLAGS])
          VLC_ADD_PLUGINS([svg]) ],
        [AC_MSG_WARN(SVG library not found)])
fi

dnl
dnl Snapshot vout module (with cache)
dnl
AC_ARG_ENABLE(snapshot,
  [  --enable-snapshot       snapshot module (default disabled)])
if test "${enable_snapshot}" = "yes"
then
  VLC_ADD_PLUGINS([snapshot])
fi

dnl
dnl  Qt Embedded module
dnl  (disabled by default)
dnl
AC_ARG_ENABLE(qte,
  [  --enable-qte            QT Embedded support (default disabled)])
if test "${enable_qte}" = "yes"
then
  AC_ARG_WITH(qte,
  [    --with-qte=PATH       Qt Embedded headers and libraries])
  if test "${with_qte}" != "no" -a -n "${with_qte}"
  then
    VLC_ADD_LIBS([qte],[-L${with_qte}/lib `echo -L${with_qte}/lib | sed 's,opt/QtPalmtop,usr,'` -ljpeg -lqte])
    VLC_ADD_CXXFLAGS([qte],[-I${with_qte}/include `echo -I${with_qte}/include | sed 's,opt/QtPalmtop,usr,'` -DQT_QWS_IPAQ -DQWS -fno-exceptions -fno-rtti])
  else
    VLC_ADD_LIBS([qte],[-L${QTDIR}/lib `echo -L${QTDIR}/lib | sed 's,opt/QtPalmtop,usr,'` -ljpeg -lqte])
    VLC_ADD_CXXFLAGS([qte],[-I${QTDIR}/include `echo -I${QTDIR}/include | sed 's,opt/QtPalmtop,usr,'` -DQT_QWS_IPAQ -DQWS -fno-exceptions -fno-rtti])
  fi
  VLC_ADD_PLUGINS([qte])
  NEED_QTE_MAIN=yes
  CPPFLAGS="${CPPFLAGS_save} ${CXXFLAGS_qte}"
  AC_CHECK_HEADERS(qt.h jpeglib.h, ,[
    AC_MSG_ERROR([echo "Cannot find QT Embedded development headers."])
  ] )
  CPPFLAGS="${CPPFLAGS_save}"
fi

dnl
dnl  Qt Video output module
dnl  (disabled by default)
dnl
dnl AC_ARG_ENABLE(qt_video,
dnl   [  --enable-qt_video            QT Video Output support (default disabled)])
dnl if test "${enable_qt_video}" = "yes"
dnl then
dnl  VLC_ADD_PLUGINS([qt_video])
dnl  VLC_ADD_LIBS([qt_video],[-L${QTDIR}/lib])
dnl  LDFLAGS="${LDFLAGS_save} ${LDFLAGS_qt_video}"
dnl   AC_CHECK_LIB(qt-mt,main,[
dnl    VLC_ADD_LIBS([qt_video],[-lqt-mt])
dnl  ],[
dnl    AC_CHECK_LIB(qt,main,[
dnl      VLC_ADD_LIBS([qt_video],[-lqt])
dnl    ])
dnl  ])
dnl  NEED_QTE_MAIN=yes
dnl  LDFLAGS="${LDFLAGS_save}"
dnl  VLC_ADD_CXXFLAGS([qt_video],[-I/usr/include/qt3 -I/usr/include/qt -I${QTDIR}/include])
dnl fi

dnl
dnl Roku HD1000 Video output module
dnl
AC_ARG_ENABLE(hd1000v,
  [  --enable-hd1000v        HD1000 Video Output module (default enabled on HD1000)])
if test "${enable_hd1000v}" != "no" -a "${CXX}" != "" &&
  (test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" ||
   test "${enable_hd1000v}" = "yes"); then
  AC_LANG_PUSH([C++])
  AC_CHECK_HEADERS([cascade/graphics/CascadeScreen.h cascade/graphics/CascadeBitmap.h],
  [
    can_build_roku="yes"
  ],
  [
    can_build_roku="no"
    AC_MSG_WARN([Not building Roku HD1000 compatible video output])
  ])
  if test "$can_build_roku" = "yes"
  then
    VLC_ADD_PLUGINS([hd1000v])
    VLC_ADD_LIBS([hd1000v],[-lCascade -ldvbpsi -lmad])
  fi
  AC_LANG_POP([C++])
fi

dnl
dnl  Windows DirectX module
dnl

if test "${SYS}" = "mingw32" -o "${SYS}" = "mingwce" -o "${SYS}" = "cygwin"
then
  VLC_ADD_PLUGINS([panoramix])
fi

AC_ARG_ENABLE(directx,
  [  --enable-directx        Win32 DirectX support (default enabled on Win32)])
if test "${enable_directx}" != "no"
then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "mingwce" -o "${SYS}" = "cygwin"
  then
    AC_ARG_WITH(directx,
    [    --with-directx=PATH   Win32 DirectX headers])
    if test -z "${with_directx}"
    then
      AC_CHECK_HEADERS(ddraw.h,
      [ VLC_ADD_PLUGINS([vout_directx aout_directx])
        VLC_ADD_LIBS([vout_directx],[-lgdi32])
      ])
      AC_CHECK_HEADERS(GL/gl.h,
      [ VLC_ADD_PLUGINS([glwin32])
        VLC_ADD_LIBS([glwin32],[-lopengl32 -lgdi32])
      ])
      AC_CHECK_HEADERS(d3d9.h,
      [ VLC_ADD_PLUGINS([direct3d])
        VLC_ADD_LIBS([direct3d],[-lgdi32])
      ])
    else
      AC_MSG_CHECKING(for directX headers in ${with_directx})
      if test -f ${with_directx}/ddraw.h
      then
        VLC_ADD_PLUGINS([vout_directx aout_directx])
        VLC_ADD_CPPFLAGS([vout_directx aout_directx],[-I${with_directx}])
        VLC_ADD_LIBS([vout_directx],[-lgdi32])
        AC_MSG_RESULT(yes)
      else
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([Cannot find ${with_directx}/ddraw.h!])
      fi
    fi
  fi
fi

dnl
dnl  Linux framebuffer module
dnl
AC_ARG_ENABLE(fb,
  [  --enable-fb             Linux framebuffer support (default enabled on Linux)])
    if test "${enable_fb}" != "no"
    then
      AC_CHECK_HEADERS(linux/fb.h, [
        VLC_ADD_PLUGINS([fb])
      ])
    fi

dnl
dnl  Linux MGA module
dnl
AC_ARG_ENABLE(mga,
  [  --enable-mga            Linux kernel Matrox support (default disabled)],
  [ if test "${enable_mga}" = "yes"
    then
      VLC_ADD_PLUGINS([mga])
    fi ])

dnl
dnl  SVGAlib module
dnl
AC_ARG_ENABLE(svgalib,
  [  --enable-svgalib        SVGAlib support (default disabled)])
if test "${enable_svgalib}" = "yes"
then
  VLC_ADD_PLUGINS([svgalib])
  VLC_ADD_LIBS([svgalib],[-lvgagl -lvga])
fi

dnl
dnl  DirectFB module
dnl  try to find using: 1 - given location; 2 - directfb-config; 3 - pkg-config
dnl  TODO: support for static linking
dnl
AC_ARG_ENABLE(directfb,
  [  --enable-directfb       DirectFB support (default disabled)])
AC_ARG_WITH(directfb, 
  [    --with-directfb=PATH  path to DirectFB headers and libraries])

if test "${enable_directfb}" = "yes"; then
    have_directfb="false"
    CPPFLAGS_mydirectfb=
    LIBS_mydirectfb=
    if test "${with_directfb}" != "no" -a -n "${with_directfb}"; then
        dnl Trying the given location
        CPPFLAGS_save="${CPPFLAGS}"
        LIBS_save="${LIBS}"

        CPPFLAGS_new="-I${with_directfb}/include -D_REENTRANT -D_GNU_SOURCE"
        LIBS_new="-L${with_directfb}/lib/fusion/.libs/ -L${with_directfb}/lib/direct/.libs/"
        LIBS_new="${LIBS_new} -L${with_directfb}/src/.libs/"

        CPPFLAGS="${CPPFLAGS} ${CPPFLAGS_new}"
        LIBS="${LIBS} ${LIBS_new}"

        dnl FIXME: too obscure
        AC_CHECK_HEADER([directfb.h], [
            AC_CHECK_LIB([direct],[direct_initialize], [
                AC_CHECK_LIB([fusion], [fusion_enter], [
                    AC_CHECK_LIB([directfb], [DirectFBInit], have_directfb="true", have_directfb="false")
                ], have_directfb="false")
            ], have_directfb="false")
        ], have_directfb="false")

        dnl Restore flags
        CPPFLAGS="${CPPFLAGS_save}"
        LIBS="${LIBS_save}"

        if test "${have_directfb}" = "true"; then
            LIBS_mydirectfb="${LIBS_new} -lz -ldl -ldirectfb -lfusion -ldirect -lpthread"
            CPPFLAGS_mydirectfb="${CPPFLAGS_new}"
        fi
    else 
        dnl Look for directfb-config
        AC_PATH_PROG(DIRECTFB_CONFIG, directfb-config, no, ${PATH})
        if test "${DIRECTFB_CONFIG}" != "no"; then
            CPPFLAGS_mydirectfb="`${DIRECTFB_CONFIG} --cflags`"
            LIBS_mydirectfb="`${DIRECTFB_CONFIG} --libs`"
            have_directfb="true"
        else 
            dnl Trying with pkg-config
            PKG_CHECK_MODULES(DIRECTFB, directfb, [
                CPPFLAGS_mydirectfb="${DIRECTFB_CFLAGS}"
                LIBS_mydirectfb="${DIRECTFB_LIBS}"
                have_directfb="true"
                ], [have_directfb="false"])
        fi
    fi
    if test "${have_directfb}" = "true"; then
        VLC_ADD_PLUGINS([directfb])
        VLC_ADD_CPPFLAGS([directfb],[${CPPFLAGS_mydirectfb}])
        VLC_ADD_LIBS([directfb],[${LIBS_mydirectfb}])
    else 
        AC_MSG_ERROR([cannot find directfb headers and/or libraries ])
    fi
fi
 


dnl
dnl  GGI module
dnl
AC_ARG_ENABLE(ggi,
  [  --enable-ggi            GGI support (default disabled)])
if test "${enable_ggi}" = "yes"
then
  VLC_ADD_PLUGINS([ggi])
  VLC_ADD_LIBS([ggi],[-lggi])
  AC_ARG_WITH(ggi,
    [    --with-ggi=PATH       path to libggi],
    [ if test "${with_ggi}" != "no" -a -n "${with_ggi}"
      then
        VLC_ADD_CPPFLAGS([ggi],[-I${with_ggi}/include])
        VLC_ADD_LIBS([ggi],[-L${with_ggi}/lib])
      fi ])
fi

dnl
dnl  Glide module
dnl
AC_ARG_ENABLE(glide,
  [  --enable-glide          Glide (3dfx) support (default disabled)])
if test "${enable_glide}" = "yes"
then
  CFLAGS_save="${CFLAGS}"
  AC_ARG_WITH(glide,
    [    --with-glide=PATH     path to libglide],
    [ if test "${with_glide}" != "no" -a -n "${with_glide}"
      then
        VLC_ADD_CPPFLAGS([glide],[-I${with_glide}/include])
        VLC_ADD_LIBS([glide],[-L${with_glide}/lib])
        CFLAGS="$CFLAGS -I${with_glide}/include"
    fi ])
   CFLAGS="$CFLAGS -I/usr/include/glide"
   AC_CHECK_HEADER(glide.h,[   
      VLC_ADD_PLUGINS([glide])
      VLC_ADD_LIBS([glide],[-lglide2x -lm])
      VLC_ADD_CPPFLAGS([glide],[-I/usr/include/glide])
    ],[
      AC_MSG_ERROR([You don't have libglide. Install it or do not use --enable-glide])
     ])
   CFLAGS="${CFLAGS_save}"
fi

dnl
dnl  AA plugin
dnl
AC_ARG_ENABLE(aa,
  [  --enable-aa             aalib output (default disabled)])
if test "${enable_aa}" = "yes"
then
  AC_CHECK_HEADER(aalib.h,have_aa="true",have_aa="false")
  if test "${have_aa}" = "true"
  then
    VLC_ADD_PLUGINS([aa])
    VLC_ADD_LIBS([aa],[-laa])
  fi
fi

dnl
dnl  libcaca plugin
dnl
AC_ARG_ENABLE(caca,
  [  --enable-caca           libcaca output (default disabled)])
if test "${enable_caca}" = "yes"
then
  CACA_PATH="${PATH}"
  AC_ARG_WITH(caca-config-path,
    [    --with-caca-config-path=PATH caca-config path (default search in \$PATH)],
    [ if test "${with_caca_config_path}" != "no"
      then
        CACA_PATH="${with_caca_config_path}:${PATH}"
      fi ])
  AC_PATH_PROG(CACA_CONFIG, caca-config, no, ${CACA_PATH})
  if test "${CACA_CONFIG}" != "no"
  then
    VLC_ADD_PLUGINS([caca])
    VLC_ADD_CFLAGS([caca],[`${CACA_CONFIG} --cflags`])
    VLC_ADD_LIBS([caca],[`${CACA_CONFIG} --plugin-libs`])
    AC_COMPILE_IFELSE( [AC_LANG_PROGRAM([[#include <caca.h>
                                        caca_event_t ev;]],
                                        [[ev.type = 0;]]
                       )],
                       [],
      [AC_DEFINE(CACA_EVENT_OPAQUE, [1], ["Define if struct caca_event is opaque and must not be accessed directly"])]
    )
  fi
fi

dnl
dnl  win32 GDI plugin
dnl
AC_ARG_ENABLE(wingdi,
  [  --enable-wingdi         Win32 GDI module (default enabled on Win32)])
if test "${enable_wingdi}" != "no"; then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"; then
    VLC_ADD_PLUGINS([wingdi])
    VLC_ADD_LIBS([wingdi],[-lgdi32])
  fi
  if test "${SYS}" = "mingwce"; then
    VLC_ADD_PLUGINS([wingdi wingapi])
  fi
fi

dnl
dnl  Audio plugins
dnl

AC_ARG_WITH(,[Audio plugins:])

dnl
dnl  OSS /dev/dsp module (enabled by default except on win32)
dnl
AC_ARG_ENABLE(oss,
  [  --enable-oss            Linux OSS /dev/dsp support (enabled on Linux)])

if test "${enable_oss}" != "no" &&
  (test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" ||
   test "${enable_oss}" = "yes")
then
  AC_CHECK_HEADERS(soundcard.h sys/soundcard.h machine/soundcard.h, [
    VLC_ADD_PLUGINS([oss])
    AC_CHECK_LIB(ossaudio,main,VLC_ADD_LIBS([oss],[-lossaudio]))
  ])
fi

dnl
dnl  Esound module
dnl
AC_ARG_ENABLE(esd,
  [  --enable-esd            Esound library support (default disabled)],
  [if test "${enable_esd}" = "yes"
   then
     AC_PATH_PROG(ESD_CONFIG, esd-config, no)
     if test "${ESD_CONFIG}" != "no"
     then
       VLC_ADD_PLUGINS([esd])
       VLC_ADD_CFLAGS([esd],[`${ESD_CONFIG} --cflags`])
       VLC_ADD_LIBS([esd],[`${ESD_CONFIG} --libs`])
     fi
   fi])

dnl
dnl  Pulseaudio module
dnl
AC_ARG_ENABLE(pulse,
  [  --enable-pulse          Pulseaudio support (default enabled)])
  if test "${enable_pulse}" != "no"
   then
     PKG_CHECK_MODULES(PULSE, libpulse >= 0.9.8,
       [ VLC_ADD_PLUGINS([pulse])
        VLC_ADD_CFLAGS([pulse],[${PULSE_CFLAGS}])
        VLC_ADD_LDFLAGS([pulse],[${PULSE_LIBS}])],
       [AC_MSG_WARN(pulsaudio library not found)])
   fi

dnl
dnl  Portaudio module
dnl
AC_ARG_ENABLE(portaudio,
  [  --enable-portaudio      Portaudio library support (default disabled)],
  [if test "${enable_portaudio}" = "yes"
   then
     VLC_ADD_PLUGINS([portaudio])
     VLC_ADD_CXXFLAGS([portaudio],[])
     if test "${SYS}" = "mingw32"; then
        VLC_ADD_LIBS([portaudio],[-lportaudio -lwinmm -lole32])
     else
        VLC_ADD_LIBS([portaudio],[-lportaudio])
     fi
   fi])

dnl
dnl  aRts module -- broken (freeze wxWidgets)
dnl
AC_ARG_ENABLE(arts,
 [  --enable-arts           aRts sound server (default disabled)],
 [if test "${enable_arts}" = "yes"
  then
    AC_PATH_PROG(ARTS_CONFIG, artsc-config, no)
    if test "${ARTS_CONFIG}" != "no"
    then
      VLC_ADD_PLUGINS([arts])
      VLC_ADD_CFLAGS([arts],[`${ARTS_CONFIG} --cflags`])
      VLC_ADD_LIBS([arts],[`${ARTS_CONFIG} --libs `])
    fi
  fi])

dnl
dnl  ALSA module
dnl
AC_ARG_ENABLE(alsa,
  [  --enable-alsa           ALSA sound support for Linux (default enabled)])
if test "${enable_alsa}" != "no"
then
  AC_CHECK_HEADER(alsa/asoundlib.h, AC_CHECK_LIB(asound, main, have_alsa="true", have_alsa="false"),have_alsa="false")
  if test "${have_alsa}" = "true"
  then
    CFLAGS="${CFLAGS_save}"
    AC_TRY_COMPILE([#define ALSA_PCM_NEW_HW_PARAMS_API
                    #define ALSA_PCM_NEW_SW_PARAMS_API
                    #include <alsa/asoundlib.h>],
       [snd_pcm_hw_params_get_period_time(0,0,0);],
        AC_DEFINE(HAVE_ALSA_NEW_API, 1, Define if ALSA is at least rc4))
    VLC_ADD_PLUGINS([alsa])
    VLC_ADD_LIBS([alsa],[-lasound -lm -ldl])
  else
    if test "${enable_alsa}" = "yes"; then
      AC_MSG_ERROR([Could not find ALSA development headers])
    fi
  fi
fi

dnl
dnl  win32 waveOut plugin
dnl
AC_ARG_ENABLE(waveout,
  [  --enable-waveout        Win32 waveOut module (default enabled on Win32)])
if test "${enable_waveout}" != "no"; then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"; then
    VLC_ADD_PLUGINS([waveout])
    VLC_ADD_LIBS([waveout],[-lwinmm])
  fi
  if test "${SYS}" = "mingwce"; then
    VLC_ADD_PLUGINS([waveout])
  fi
fi

dnl
dnl  CoreAudio plugin
dnl
AC_ARG_ENABLE(macosx-audio,
  [  --enable-macosx-audio   Mac OS X audio module (default enabled on MacOS X)])
if test "${enable_macosx-audio}" != "no" &&
  (test "${SYS}" = "darwin" || test "${enable_macosx-audio}" = "yes")
then
  AC_CHECK_HEADERS(CoreAudio/CoreAudio.h, 
    [ VLC_ADD_BUILTINS([auhal])
      VLC_ADD_LDFLAGS([auhal],[-Wl,-framework,CoreAudio,-framework,AudioUnit,-framework,AudioToolbox,-framework,Carbon])
    ], [ AC_MSG_ERROR([cannot find CoreAudio headers]) ])
fi

dnl
dnl  Roku HD1000 audio
dnl
AC_ARG_ENABLE(hd1000a,
  [  --enable-hd1000a        HD1000 audio module (default enabled on HD1000)])
if test "${enable_hd1000a}" != "no" -a "${CXX}" != "" &&
  (test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" ||
   test "${enable_hd1000a}" = "yes")
then
  AC_LANG_PUSH([C++])
  AC_CHECK_HEADERS(deschutes/libraries/hdmachinex225/PCMAudioPlayer.h, [
    VLC_ADD_PLUGINS([hd1000a])
    AC_CHECK_LIB(HDMachineX225,main,VLC_ADD_LIBS([hd1000a],[-lHDMachineX225]))  ])
  AC_LANG_POP([C++])
fi

dnl
dnl  JACK modules
dnl
AC_ARG_ENABLE(jack,
 [  --enable-jack           JACK audio I/O modules (default disabled)],,
 [enable_jack="no"])

AS_IF([test "${enable_jack}" != "no"], [
  AC_CHECK_HEADERS(jack/jack.h, [
    VLC_ADD_PLUGINS([access_jack jack])
    VLC_ADD_LIBS([access_jack jack],[-ljack])
  ],[AC_MSG_ERROR([cannot find JACK headers])])
])

dnl
dnl  CyberLink for C++ UPnP stack
dnl
AC_ARG_ENABLE(cyberlink,
  [  --enable-cyberlink      CyberLink for C++ UPnP stack (default disabled)])
  AS_IF([test "${enable_cyberlink}" = "yes" ], [
  AC_ARG_WITH(cyberlink-tree,
    [    --with-cyberlink-tree=PATH CyberLink for C++ tree for static linking])

  dnl
  dnl test for --with-cyberlink-tree
  dnl
  AS_IF([test ! -z "${with_cyberlink_tree}" -a "${CXX}" != ""], [
    AC_LANG_PUSH(C++)
    real_cyberlink_tree="`cd ${with_cyberlink_tree} 2>/dev/null && pwd`"
    AS_IF([test -z "${real_cyberlink_tree}"], [
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_cyberlink_tree}])
    ])
    CPPFLAGS_save="${CPPFLAGS}"
    CPPFLAGS_cyberlink="-I${real_cyberlink_tree}/include"
    CPPFLAGS="${CPPFLAGS} ${CPPFLAGS_cyberlink}"
    AC_CHECK_HEADERS([cybergarage/upnp/MediaServer.h],
      [ VLC_ADD_CXXFLAGS([upnp_cc], [${CPPFLAGS_cyberlink}])
        VLC_ADD_PLUGINS([upnp_cc]) 
      ],[
        AC_MSG_ERROR([cannot find CyberLink for C++ headers])
      ])
    AC_MSG_CHECKING(for libclink.a in ${with_cyberlink_tree})
    AS_IF([test -f "${real_cyberlink_tree}/lib/unix/libclink.a"], [
      AC_MSG_RESULT(${real_cyberlink_tree}/lib/unix/libclink.a)
      dnl The mere fact that we have to make such an ugly check sucks
      AC_MSG_CHECKING(for XML parser to link CyberLink with)
      LIBS_save="$LIBS"
      LIBS_cclink="no"
      for l in "`xml2-config --libs`" -lexpat -lxerces-c; do
        LIBS="$LIBS_save ${real_cyberlink_tree}/lib/unix/libclink.a -lpthread $l"
        AC_LINK_IFELSE([AC_LANG_PROGRAM([
#include <cybergarage/upnp/media/player/MediaPlayer.h>
using namespace CyberLink;

class testclass : public SearchResponseListener, public MediaPlayer
{
    virtual void deviceSearchResponseReceived( SSDPPacket *)
    {
    }

    public:
      testclass (void)
      {
        addSearchResponseListener (this);
	start ();
      }
};
],[testclass l;])],[LIBS_cclink="$l"])
      done
      LIBS="${LIBS_save}"
      dnl should not happen - otherwise this needs fixing - hence FAILURE
      AS_IF([test "${LIBS_cclink}" = "no"],
        [AC_MSG_FAILURE([cannot find XML parser for CyberLink])])
      AC_MSG_RESULT([${LIBS_cclink}])
      VLC_ADD_LIBS([upnp_cc], [${real_cyberlink_tree}/lib/unix/libclink.a -lpthread ${LIBS_cclink}])
    ], [
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_cyberlink_tree}/lib/unix/libclink.a, make sure you compiled CyberLink for C++ in ${with_cyberlink_tree}])
    ])
    CPPFLAGS="${CPPFLAGS_save}"
    AC_LANG_POP([C++])
  ])
])

dnl
dnl UPnP Plugin (Intel SDK)
dnl
AC_ARG_ENABLE(upnp,
  [  --enable-upnp           Intel UPnP SDK (default enabled)])

VLC_ADD_CXXFLAGS([upnp_intel], [ ])
AS_IF([test "x${enable_upnp}" != "xno"], [
  AC_CHECK_LIB([upnp], [UpnpInit], [has_upnp="yes"], [has_upnp="no"], [-lpthread])
  AS_IF([test "x${enable_upnp}" != "x" && test "${has_upnp}" = "no"], [
    AC_MSG_ERROR([cannot find Intel UPnP SDK (libupnp)])
  ])
  AS_IF([test "${has_upnp}" = "yes"], [
    VLC_ADD_LIBS([upnp_intel], [-lupnp -lixml])
  ])
], [
  has_upnp="no"
])

AS_IF([test "${has_upnp}" = "yes"], [
  VLC_ADD_PLUGINS([upnp_intel])
])


dnl
dnl  Interface plugins
dnl

AC_ARG_WITH(,[Interface plugins:])

dnl special case for BeOS
if test "${SYS}" = "beos"
then
    VLC_ADD_BUILTINS([beos])
fi

dnl
dnl Skins2 module
dnl
AC_ARG_ENABLE(skins2,
  [  --enable-skins2         Skins2 interface module (default disabled)])
if test "${enable_skins2}" = "yes" ||
  (test "${SYS}" != "darwin" && test "${SYS}" != "beos" &&
   test "${SYS}" != "mingwce" && test "${enable_skins2}" != "no"); then

  dnl test for the required libraries
  skins2_missing_lib="no"

  dnl freetype
  if test "${have_freetype}" != "yes"; then
    skins2_missing_lib="yes"
    if test "${enable_skins2}" = "yes"; then
      AC_MSG_ERROR([Could not find freetype (required for skins2)])
    fi
  fi

  if test "${skins2_missing_lib}" = "no" && (test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"); then
    VLC_ADD_PLUGINS([skins2])
    ALIASES="${ALIASES} svlc"
    VLC_ADD_CPPFLAGS([skins2],[-U_OFF_T_ -U_off_t -Imodules/gui/skins2 -DWIN32_SKINS])
    VLC_ADD_CXXFLAGS([skins2],[-O2 -fno-rtti])
    VLC_ADD_LIBS([skins2],[-loleaut32 -lwinspool -lwinmm -lshell32 -lctl3d32 -ladvapi32 -lwsock32 -lgdi32 -lcomdlg32 -lole32 -luuid -lcomctl32])

  else if test "${skins2_missing_lib}" = "no" && (test "${SYS}" = "darwin"); then
    VLC_ADD_PLUGINS([skins2])
    ALIASES="${ALIASES} svlc"
    VLC_ADD_CPPFLAGS([skins2],[-Imodules/gui/skins2 -DMACOSX_SKINS])
    VLC_ADD_CXXFLAGS([skins2],[-O2 -fno-rtti])
    VLC_ADD_LDFLAGS([skins2],[-Wl,-framework,Carbon])

  else if test "${skins2_missing_lib}" = "no"; then
    VLC_ADD_PLUGINS([skins2])
    ALIASES="${ALIASES} svlc"
    VLC_ADD_CPPFLAGS([skins2],[-Imodules/gui/skins2 ${X_CFLAGS} -DX11_SKINS])
    VLC_ADD_CXXFLAGS([skins2],[-O2 -fno-rtti])
    VLC_ADD_LIBS([skins2],[${X_LIBS} ${X_PRE_LIBS} -lXext -lX11])
  fi fi fi
fi


dnl dnl
dnl dnl  Gtk+ module
dnl dnl
dnl AC_ARG_ENABLE(gtk,
dnl   [  --enable-gtk            Gtk+ support (default enabled)])
dnl if test "${enable_gtk}" != "no"
dnl then
dnl   GTK_PATH="${PATH}"
dnl   AC_ARG_WITH(gtk-config-path,
dnl     [    --with-gtk-config-path=PATH gtk-config path (default search in \$PATH)],
dnl     [ if test "${with_gtk_config_path}" != "no"
dnl       then
dnl         GTK_PATH="${with_gtk_config_path}:${PATH}"
dnl       fi ])
dnl   # look for gtk-config
dnl   AC_PATH_PROG(GTK12_CONFIG, gtk12-config, no, ${GTK_PATH})
dnl   GTK_CONFIG=${GTK12_CONFIG}
dnl   if test "${GTK_CONFIG}" = "no"
dnl   then
dnl     AC_PATH_PROG(GTK_CONFIG, gtk-config, no, ${GTK_PATH})
dnl   fi
dnl   if test "${GTK_CONFIG}" != "no"
dnl   then
dnl     if expr 1.2.0 \> `${GTK_CONFIG} --version` >/dev/null
dnl     then
dnl       AC_MSG_ERROR([Your development package for Gtk+ is too old, you need at least version 1.2.0. Please upgrade and try again. Alternatively you can also configure with --disable-gtk.])
dnl     fi
dnl     if test "${SYS}" != "mingw32"; then
dnl       VLC_ADD_CFLAGS([gtk],[`${GTK_CONFIG} --cflags gtk gthread`])
dnl       VLC_ADD_LIBS([gtk],[`${GTK_CONFIG} --libs gtk gthread | sed 's,-rdynamic,,'`])
dnl     else
dnl       VLC_ADD_CFLAGS([gtk],[`${GTK_CONFIG} --cflags gtk`])
dnl       VLC_ADD_LIBS([gtk],[`${GTK_CONFIG} --libs gtk | sed 's,-rdynamic,,'`])
dnl     fi
dnl     # now look for the gtk.h header
dnl     CPPFLAGS="${CPPFLAGS_save} ${CFLAGS_gtk}"
dnl     ac_cv_gtk_headers=yes
dnl     AC_CHECK_HEADERS(gtk/gtk.h glib.h gdk/gdk.h, , [
dnl       ac_cv_gtk_headers=no
dnl       echo "Cannot find gtk development headers."
dnl     ])
dnl     if test "${ac_cv_gtk_headers}" = "yes"
dnl     then
dnl       VLC_ADD_PLUGINS([gtk])
dnl       if test "${SYS}" != "mingw32"; then
dnl         NEED_GTK_MAIN=yes
dnl       fi
dnl       ALIASES="${ALIASES} gvlc"
dnl     fi
dnl     CPPFLAGS="${CPPFLAGS_save}"
dnl   fi
dnl fi
dnl 
dnl
dnl  Gtk+2 module ! Disabled for now as it is unusable and confuses users
dnl
dnl AC_ARG_ENABLE(gtk2,
dnl   [  --enable-gtk2           Gtk2 support (default disabled)])
dnl if test "${enable_gtk2}" = "yes"
dnl then
dnl   PKG_CHECK_MODULES(GTK2, [gtk+-2.0 >= 2.0.0, gthread-2.0])
dnl   VLC_ADD_CFLAGS([gtk2],[${GTK2_CFLAGS}])
dnl   VLC_ADD_LIBS([gtk2],[${GTK2_LIBS}])
dnl   VLC_ADD_PLUGINS([gtk2])
dnl   if test "${SYS}" != "mingw32"; then
dnl     NEED_GTK2_MAIN=yes
dnl   fi
dnl fi

dnl
dnl  PDA Gtk+2 module
dnl
AC_ARG_ENABLE(pda,
  [  --enable-pda            PDA interface needs Gtk2 support (default disabled)])
if test "${enable_pda}" = "yes"
then
  PKG_CHECK_MODULES(GTK2, [gtk+-2.0 >= 2.0.0, gthread-2.0])
  VLC_ADD_CFLAGS([gtk2],[${GTK2_CFLAGS}])
  VLC_ADD_LIBS([gtk2],[${GTK2_LIBS}])
  VLC_ADD_CFLAGS([pda],[${GTK2_CFLAGS} ${CFLAGS_pda}])
  VLC_ADD_LIBS([pda],[${LDFLAGS_pda}])
  VLC_ADD_LIBS([pda],[${GTK2_LIBS}])
  VLC_ADD_PLUGINS([pda])
  if test "${SYS}" != "mingw32"; then
    NEED_GTK2_MAIN=yes
  fi
fi

dnl dnl
dnl dnl  Gnome module
dnl dnl
dnl AC_ARG_ENABLE(gnome,
dnl   [  --enable-gnome          Gnome interface support (default disabled)],
dnl   [if test "${enable_gnome}" = "yes"; then
dnl     # look for gnome-config
dnl     AC_PATH_PROG(GNOME_CONFIG, gnome-config, no)
dnl     if test -x ${GNOME_CONFIG}
dnl     then
dnl        VLC_ADD_CFLAGS([gnome],[`${GNOME_CONFIG} --cflags gtk gnomeui`])
dnl        VLC_ADD_LIBS([gnome],[`${GNOME_CONFIG} --libs gnomeui | sed 's,-rdynamic,,'`])
dnl     fi
dnl     # now look for the gnome.h header
dnl     CPPFLAGS="${CPPFLAGS_save} ${CFLAGS_gnome}"
dnl     AC_CHECK_HEADERS(gnome.h, [
dnl       VLC_ADD_PLUGINS([gnome])
dnl       NEED_GTK_MAIN=yes
dnl       NEED_GNOME_MAIN=yes
dnl       ALIASES="${ALIASES} gnome-vlc"
dnl       dnl We need this because of some moronic gnomesupport.h flavours
dnl       AC_MSG_CHECKING(for strndup in gnome.h)
dnl       AC_EGREP_HEADER(strndup,gnome.h,[
dnl         AC_MSG_RESULT(yes)
dnl         AC_DEFINE(STRNDUP_IN_GNOME_H, 1,
dnl                   Define if <gnome.h> defines strndup.)],[
dnl         AC_MSG_RESULT(no)])
dnl      ],[
dnl       AC_MSG_ERROR([Can't find gnome headers. Please install the gnome
dnl developement tools or remove the --enable-gnome option])
dnl      ])
dnl     CPPFLAGS="${CPPFLAGS_save}"
dnl   fi])

dnl
dnl  Gnome2 module ! Disabled for know as it is unuseable and confuses users
dnl
dnl AC_ARG_ENABLE(gnome2,
dnl   [  --enable-gnome2         Gnome2 support (default disabled)])
dnl if test "${enable_gnome2}" = "yes"
dnl then
dnl   PKG_CHECK_MODULES(GNOME2, [libgnomeui-2.0])
dnl   VLC_ADD_CFLAGS([gnome2],[${GNOME2_CFLAGS}])
dnl   VLC_ADD_LIBS([gnome2],[${GNOME2_LIBS}])
dnl   VLC_ADD_PLUGINS([gnome2])
dnl   if test "${SYS}" != "mingw32"; then
dnl     NEED_GNOME2_MAIN=yes
dnl   fi
dnl fi

dnl
dnl  wxWidgets module
dnl
AC_ARG_ENABLE(wxwidgets,
  [  --enable-wxwidgets      wxWidgets support (default disabled)])

if test "${enable_wxwidgets}" = "yes"
then
  AC_MSG_WARN([wxWidgets support is deprecated and will be removed. Consider using Qt4 instead.])
  WXWIDGETS_PATH="${PATH}"
  AC_ARG_WITH(wx-config-path,
    [    --with-wx-config-path=PATH wx-config path (default search in \$PATH)],
    [ if test "${with_wx_config_path}" != "no"
      then
        WXWIDGETS_PATH="${with_wx_config_path}:${PATH}"
      fi ])
  WXWIDGETS_NAME="wx-config"
  AC_ARG_WITH(wx-config,
    [    --with-wx-config=NAME      wx-config name (default is wx-config)],
    [ if test "${with_wx_config}" != "no"
      then
        WXWIDGETS_NAME="${with_wx_config}"
      fi ])
  # look for wx-config
  AC_PATH_PROG(WX_CONFIG, ${WXWIDGETS_NAME}, no, ${WXWIDGETS_PATH})
  if test "${WX_CONFIG}" != "no" -a "${CXX}" != ""
  then
    if expr 2.6.0 \> `${WX_CONFIG} --version` >/dev/null
    then
      AC_MSG_ERROR([You need wxWidgets version 2.6.0 or upwards. Please upgrade and try again.])
    fi
    AC_LANG_PUSH(C++)
    # Turn this error:
    #   playlist.cpp:1351: error: ISO C++ forbids cast to non-reference type
    # into a warning. However better would be to fix playlist.cpp
    AC_CACHE_CHECK([if \$CXX accepts -fpermissive],
        [ac_cv_cxx_fpermissive],
        [CXXFLAGS="${CXXFLAGS_save} -fpermissive"
         AC_TRY_COMPILE([],,ac_cv_cxx_fpermissive=yes,
                        ac_cv_cxx_fpermissive=no)])
    if test "${ac_cv_cxx_fpermissive}" = "yes"; then
      VLC_ADD_CXXFLAGS([wxwidgets],-fpermissive)
    fi
    VLC_ADD_LIBS([wxwidgets],[`${WX_CONFIG} --libs`])
    VLC_ADD_CXXFLAGS([wxwidgets],[`${WX_CONFIG} --cxxflags`])
    # check if we must link against gtk2 libs
    # if __WXGTK20__ && __WXGTK__ are defined
    # for modules/gui/wxwidgets/video.cpp
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
#include <wx/wx.h>
#ifdef __WXGTK20__
#error ok, we need to link against gtk2
#endif
        ])],[WXGTK=no],[WXGTK=yes])
    if test "$WXGTK" = "yes"; then
      ${PKG_CONFIG} --exists gtk+-2.0 && VLC_ADD_LIBS([wxwidgets],[`$PKG_CONFIG --libs gtk+-2.0`])
    fi
    if ${WX_CONFIG} --unicode
    then 
      # wxwidgets should provide the following flags but does not
      # the following is required to compile for win32
      VLC_ADD_CXXFLAGS([wxwidgets],[-D_UNICODE -DUNICODE])
    else
      AC_MSG_ERROR([You need a unicode build of wxWidgets. Please fix that and try again. Better yet, you can configure with --disable-wxwidgets.])
    fi
    if test "$have_libcdio" = "yes"
    then 
      VLC_ADD_LIBS([wxwidgets],[$LIBCDIO_LIBS])
      VLC_ADD_CXXFLAGS([wxwidgets],[$LIBCDIO_CFLAGS])
    else 
      AC_MSG_WARN([Probe disc disabled because ok libcdio library not found])
    fi

    if test "$have_libvcdinfo" = "yes"
    then 
      VLC_ADD_LIBS([wxwidgets],[$VCDINFO_LIBS])
      VLC_ADD_CXXFLAGS([wxwidgets],[$VCDINFO_CFLAGS])
    else 
      AC_MSG_WARN([VCD information on Probe disc disabled because ok libvcdinfo not found])
    fi

    # now look for the wxprec.h header
    CPPFLAGS="${CPPFLAGS_save} ${CXXFLAGS_wxwidgets}"
    ac_cv_wx_headers=yes
    AC_CHECK_HEADERS(wx/wxprec.h, , [
      ac_cv_wx_headers=no
      echo "Cannot find wxWidgets development headers."
    ])
    if test "${ac_cv_wx_headers}" = "yes"
    then
      VLC_ADD_PLUGINS([wxwidgets])
      AC_DEFINE([HAVE_WX], 1, [Define to 1 if you have WxWidgets library.])
      ALIASES="${ALIASES} wxvlc"
    fi
    CPPFLAGS="${CPPFLAGS_save}"
    AC_LANG_POP(C++)
  fi
  dnl WxWidgets plugin will yield incorrect code without this
  VLC_ADD_CXXFLAGS([wxwidgets], [-fno-strict-aliasing])
fi

dnl
dnl QT 4
dnl
enableqt4=false
AC_ARG_ENABLE(qt4,
  [  --enable-qt4            QT 4 support (default enabled) ])
AS_IF([test "${enable_qt4}" != "no" &&
  (test "${SYS}" != "darwin" || test "${enable_qt4}" = "yes")], [
  PKG_CHECK_MODULES(QT4, [QtCore QtGui >= 4.2.0],
    [ VLC_ADD_PLUGINS([qt4])
      AC_DEFINE([HAVE_QT4], 1, [Define to 1 if you have QT4 library.])
      ALIASES="${ALIASES} qvlc"
      enableqt4=true
      if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" -a "${SYS}" != "cygwin" -a "${SYS}" != "darwin"; then
          VLC_ADD_LIBS([qt4],[$QT4_LIBS -lX11])
      else
          VLC_ADD_LIBS([qt4],[$QT4_LIBS])
      fi
      VLC_ADD_CXXFLAGS([qt4],[$QT4_CFLAGS -DQT4LOCALEDIR=\\\\\\\"$($PKG_CONFIG --variable=prefix QtCore)/share/qt4/translations/\\\\\\\"])
      AC_PATH_PROGS(MOC, [moc-qt4 moc], moc,`$PKG_CONFIG --variable=exec_prefix QtCore`/bin)
      AC_PATH_PROG(RCC, rcc, rcc,`$PKG_CONFIG --variable=exec_prefix QtCore`/bin)
      AC_PATH_PROGS(UIC, [uic-qt4 uic], uic,`$PKG_CONFIG --variable=exec_prefix QtCore`/bin)
      ],
      AS_IF([test "${enable_qt4}" = "yes"],[
    AC_MSG_ERROR(QT4 library not found)
      ],[
    AC_MSG_WARN(QT4 library not found)
      ])
    )
])
AM_CONDITIONAL(ENABLE_QT4, test "$enableqt4" = "true")

dnl
dnl  WinCE GUI module
dnl
if test "${SYS}" = "mingwce"; then
  VLC_ADD_BUILTINS([wince])
  VLC_ADD_CXXFLAGS([wince],[])
  VLC_ADD_LIBS([wince],[-lcommctrl -lcommdlg -laygshell])
  dnl Gross hack
  VLC_ADD_LIBS([wince],[\\\${top_builddir}modules/gui/wince/wince_rc.o])
elif test "${SYS}" = "mingw32"; then
  VLC_ADD_CXXFLAGS([wince],[])
  VLC_ADD_LIBS([wince],[-lcomctl32 -lcomdlg32 -lgdi32 -lole32])
  dnl Gross hack
  VLC_ADD_LIBS([wince],[\\\${top_builddir}modules/gui/wince/wince_rc.o])
fi

dnl
dnl Simple test for skins2 dependency
dnl
if test "${enable_skins2}" != "no"
then
  if test "${WX_CONFIG}" = "no"
  then
    AC_MSG_ERROR([The skins2 module depends on the wxWidgets development package. Without it you won't be able to open any dialog box from the interface, which makes the skins2 interface rather useless. Install the wxWidgets development package or alternatively you can also configure with: --disable-wxwidgets --disable-skins2.])
  fi
fi

dnl dnl
dnl dnl  Qt module
dnl dnl
dnl AC_ARG_ENABLE(qt,
dnl   [  --enable-qt             Qt interface support (default disabled)],
dnl   [if test "${enable_qt}" = "yes"; then
dnl      VLC_ADD_PLUGINS([qt])
dnl      ALIASES="${ALIASES} qvlc"
dnl      VLC_ADD_LIBS([qt],[-L${QTDIR}/lib])
dnl      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_qt}"
dnl      AC_CHECK_LIB(qt-mt,main,[
dnl        VLC_ADD_LIBS([qt],[-lqt-mt])
dnl      ],[
dnl        AC_CHECK_LIB(qt,main,[
dnl          VLC_ADD_LIBS([qt],[-lqt])
dnl        ])
dnl      ])
dnl      LDFLAGS="${LDFLAGS_save}"
dnl      VLC_ADD_CXXFLAGS([qt],[-I/usr/include/qt3 -I/usr/include/qt -I${QTDIR}/include])
dnl      if test -x ${QTDIR}/bin/moc
dnl      then
dnl        MOC=${QTDIR}/bin/moc
dnl      else
dnl        MOC=moc
dnl      fi
dnl    fi])
dnl 
dnl dnl
dnl dnl  KDE module
dnl dnl
dnl AC_ARG_ENABLE(kde,
dnl   [  --enable-kde            KDE interface support (default disabled)],
dnl   [if test "${enable_kde}" = "yes"; then
dnl      VLC_ADD_PLUGINS([kde])
dnl      ALIASES="${ALIASES} kvlc"
dnl      VLC_ADD_LIBS([kde],[-L${KDEDIR}/lib])
dnl      dnl Check for -lkfile (only in KDE 2) or -lkdeui -lkio (KDE 3)
dnl      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_kde}"
dnl      AC_CHECK_LIB(kfile,main,[
dnl        VLC_ADD_LIBS([kde],[-lkfile])
dnl      ])
dnl      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_kde}"
dnl      AC_CHECK_LIB(kdeui,main,[
dnl        VLC_ADD_LIBS([kde],[-lkdeui])
dnl      ])
dnl      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_kde}"
dnl      AC_CHECK_LIB(kio,main,[
dnl        VLC_ADD_LIBS([kde],[-lkio])
dnl      ])
dnl      LDFLAGS="${LDFLAGS_save}"
dnl      VLC_ADD_CXXFLAGS([kde],[-I/usr/include/kde -I/usr/include/qt3 -I/usr/include/qt])
dnl      VLC_ADD_CXXFLAGS([kde],[-I${KDEDIR}/include -I${QTDIR}/include])
dnl      if test -x ${QTDIR}/bin/moc
dnl      then
dnl        MOC=${QTDIR}/bin/moc
dnl      else
dnl        MOC=moc
dnl      fi
dnl    fi])

dnl
dnl  Opie QT embedded module
dnl
AC_ARG_ENABLE(opie,
  [  --enable-opie           Qt embedded interface support (default disabled)],
  [if test "${enable_opie}" = "yes"; then
     AC_ARG_WITH(qte,
     [    --with-qte=PATH       Qt Embedded headers and libraries])
     if test "${with_qte}" != "no" -a -n "${with_qte}"
     then
       VLC_ADD_LIBS([qte],[-L${with_qte}/lib `echo -L${with_qte}/lib | sed 's,opt/QtPalmtop,usr,'` -ljpeg -lqte])
       VLC_ADD_CXXFLAGS([qte],[-I${with_qte}/include `echo -I${with_qte}/include | sed 's,opt/QtPalmtop,usr,'` -DQT_QWS_IPAQ -DQWS -fno-exceptions -fno-rtti])
     else
       VLC_ADD_LIBS([qte],[-L${QTDIR}/lib `echo -L${QTDIR}/lib | sed 's,opt/QtPalmtop,usr,'`])
       VLC_ADD_CXXFLAGS([qte],[-I${QTDIR}/include `echo -I${QTDIR}/include | sed 's,opt/QtPalmtop,usr,'`])
     fi
     CPPFLAGS="${CPPFLAGS_save} ${CXXFLAGS_qte}"
     AC_CHECK_HEADERS(qt.h jpeglib.h, ,[
       AC_MSG_ERROR([echo "Cannot find QT Embedded development headers."])
     ] )
     CPPFLAGS="${CPPFLAGS_save}"

     VLC_ADD_PLUGINS([opie])
     NEED_QTE_MAIN=yes
     VLC_ADD_LIBS([opie],[-lqpe ${LDFLAGS_qte}])
     VLC_ADD_CXXFLAGS([opie],[${CXXFLAGS_qte}])
     if test "${with_qte}" != "no" -a -n "${with_qte}"
     then
       MOC=${with_qte}/bin/moc
     else
       MOC=${QTDIR}/bin/moc
     fi
   fi])

dnl
dnl  MacOS X video output/gui modules
dnl
AC_ARG_ENABLE(macosx,
  [  --enable-macosx         MacOS X support (default enabled on MacOS X)])
if test "x${enable_macosx}" = "xyes"
then
#  VLC_ADD_LDFLAGS([access_eyetv],                     [-Wl,-framework,Foundation])
  VLC_ADD_LDFLAGS([macosx minimal_macosx opengllayer],[-Wl,-framework,Cocoa])
  VLC_ADD_LDFLAGS([macosx minimal_macosx opengllayer],[-Wl,-framework,OpenGL])
  VLC_ADD_LDFLAGS([macosx minimal_macosx],            [-Wl,-framework,Carbon])
  VLC_ADD_LDFLAGS([macosx minimal_macosx],            [-Wl,-framework,AGL])
  VLC_ADD_LDFLAGS([macosx],                           [-Wl,-framework,IOKit])
  VLC_ADD_LDFLAGS([macosx],                           [-Wl,-framework,QuickTime])
  VLC_ADD_LDFLAGS([macosx],                           [-Wl,-framework,QTKit])
  VLC_ADD_LDFLAGS([macosx],                           [-Wl,-framework,WebKit])
  VLC_ADD_LDFLAGS([opengllayer],                      [-Wl,-framework,QuartzCore])
  VLC_ADD_OBJCFLAGS([macosx minimal_macosx opengllayer growl], [-fobjc-exceptions] )

#  VLC_ADD_PLUGINS([access_eyetv])
  VLC_ADD_BUILTINS([macosx minimal_macosx])
 
  ORIGCFLAGS=$CFLAGS
  CFLAGS="$CFLAGS -x objective-c"
  AC_CHECK_HEADER(QuartzCore/CALayer.h, [VLC_ADD_BUILTINS([opengllayer])])
  CFLAGS=$ORIGCFLAGS
fi

dnl
dnl  QNX RTOS module
dnl
AC_ARG_ENABLE(qnx,
  [  --enable-qnx            QNX RTOS support (default enabled on QNX RTOS)])
    if test "${enable_qnx}" != "no"
    then
      AC_CHECK_HEADERS(Ph.h, [
        VLC_ADD_PLUGINS([qnx])
        VLC_ADD_LIBS([qnx],[-lasound -lph])
      ])
    fi

dnl
dnl  ncurses module
dnl
AC_ARG_ENABLE(ncurses,
  [  --disable-ncurses       ncurses interface support (default enabled)],
  [if test "${enable_ncurses}" != "no"; then
    AC_CHECK_HEADER(ncurses.h,
      [AC_CHECK_LIB(ncursesw, mvprintw,
        [VLC_ADD_PLUGINS([ncurses])
        VLC_ADD_LIBS([ncurses],[-lncursesw])
        ALIASES="${ALIASES} nvlc"
        AC_DEFINE([HAVE_NCURSESW], 1, [Define to 1 if you have libncursesw.])
        ],
        [AC_CHECK_LIB( ncurses, mvprintw,
          [VLC_ADD_PLUGINS([ncurses])
          ALIASES="${ALIASES} nvlc"
          VLC_ADD_LIBS([ncurses],[-lncurses])],
          [AS_IF([test "x${enable_ncurses}" != "x"], [
            AC_MSG_ERROR([libncurses not found])])]
        )]
      )]
    )
  fi]
)

dnl
dnl  XOSD plugin
dnl
AC_ARG_ENABLE(xosd,
  [  --enable-xosd           xosd interface support (default disabled)])
if test "${enable_xosd}" = "yes"
then
  AC_CHECK_HEADER(xosd.h, have_xosd="true", have_xosd="false")
  AC_CHECK_LIB(xosd,xosd_set_offset,
      AC_DEFINE(HAVE_XOSD_VERSION_1, 1, Define if <xosd.h> is 1.0.x),
    AC_CHECK_LIB(xosd,xosd_set_horizontal_offset,
        AC_DEFINE(HAVE_XOSD_VERSION_2, 1, Define if <xosd.h> is 2.0.x),
      AC_TRY_COMPILE([#include <xosd.h>],
         [xosd_init("foo","bar",12,XOSD_top,2,12,42);],,
          AC_DEFINE(HAVE_XOSD_VERSION_0, 1, Define if <xosd.h> is pre-1.0.0))))
  if test "${have_xosd}" = "true"
  then
    VLC_ADD_PLUGINS([xosd])
    VLC_ADD_LIBS([xosd],[-lxosd])
  fi
fi

dnl
dnl Framebuffer (overlay) plugin
dnl
AC_ARG_ENABLE(fbosd,
  [  --enable-fbosd          fbosd interface support (default disabled)])
if test "${enable_fbosd}" = "yes"
then
  AC_CHECK_HEADERS(linux/fb.h, [
    VLC_ADD_PLUGINS([fbosd])
 ])
fi

dnl
dnl Visualisation plugin
dnl
AC_ARG_ENABLE(visual,
  [  --enable-visual         visualisation plugin (default enabled)])
if test "${enable_visual}" != "no"
then
    VLC_ADD_PLUGINS([visual])
fi

dnl
dnl OpenGL visualisation plugin
dnl
AC_ARG_ENABLE(galaktos,
  [  --enable-galaktos       OpenGL visualisation plugin (default disabled)])
if test "${enable_galaktos}" = "yes"
then
  AC_CHECK_HEADERS(GL/gl.h GL/glu.h, [
    VLC_ADD_PLUGINS([galaktos])
    if test "${SYS}" != "mingw32"; then
      VLC_ADD_LIBS([galaktos],[${X_LIBS} -lGL -lGLU])
    else
      VLC_ADD_LIBS([galaktos],[-lopengl32])
    fi
  ])
fi

dnl
dnl  goom visualization plugin
dnl
AC_ARG_ENABLE(goom,
[  --enable-goom           goom visualisation plugin (default disabled)])
if test "${enable_goom}" = "yes"
then
  AC_ARG_WITH(goom-tree,
    [    --with-goom-tree=PATH goom tree for static linking (required)])

  dnl
  dnl test for --with-goom-tree
  dnl
  if test "${with_goom_tree}" != "no" -a -n "${with_goom_tree}"; then
    AC_MSG_CHECKING(for libgoom2.a in ${with_goom_tree})
    real_goom_tree="`cd ${with_goom_tree} 2>/dev/null && pwd`"
    if test -z "${real_goom_tree}"; then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_goom_tree}])
    fi
    if test -f "${real_goom_tree}/src/.libs/libgoom2.a"; then
      AC_MSG_RESULT(${real_goom_tree}/src/.libs/libgoom2.a)
      VLC_ADD_BUILTINS([goom])
      VLC_ADD_LIBS([goom],[-L${real_goom_tree}/src/.libs -lgoom2])
      VLC_ADD_CPPFLAGS([goom],[-I${real_goom_tree}/src -DUSE_GOOM_TREE])
    else
      dnl  The given libgoom2 wasn't built, try to look for the old goom
      AC_MSG_RESULT(no)
      AC_MSG_CHECKING(for libgoom.a in ${with_goom_tree})
      if test -f "${real_goom_tree}/libgoom.a"; then
        AC_MSG_RESULT(${real_goom_tree}/libgoom.a)
        VLC_ADD_BUILTINS([goom])
        VLC_ADD_LIBS([goom],[-L${real_goom_tree} -lgoom])
        VLC_ADD_CPPFLAGS([goom],[-I${real_goom_tree} -DUSE_GOOM_TREE -DOLD_GOOM])
      else
        dnl  The given libgoom wasn't built
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot find ${real_goom_tree}/src/.libs/libgoom2.a, make sure you compiled goom in ${with_goom_tree}])
      fi
    fi
  else
    AC_CHECK_HEADERS(goom/goom.h, [
      LDFLAGS="${LDFLAGS_save} ${LIBS_goom}"
      AC_CHECK_LIB(goom2, goom_init, [
        VLC_ADD_PLUGINS([goom])
        VLC_ADD_LIBS([goom],[-lgoom2])
      ],[
        AC_MSG_ERROR([Could not find goom on your system: you may get it from http://www.ios-software.com/.])
      ])
      LDFLAGS="${LDFLAGS_save}"
    ])
  fi
fi

dnl
dnl  AtmoLight (homebrew AmbiLight)
dnl
if test "${SYS}" = "mingw32" -o "${SYS}" = "linux"; then
   AC_ARG_ENABLE(atmo,
[  --disable-atmo	AtmoLight (homebrew philips ambilight) (default enabled)])
   if test "${enable_atmo}" != "no"; then
      AC_LANG_PUSH(C++)
      VLC_ADD_PLUGINS([atmo])
      AC_LANG_POP(C++)
   fi
fi

dnl
dnl  Bonjour services discovery
dnl
AC_ARG_ENABLE(bonjour,
  [  --enable-bonjour        Bonjour services discovery (default enabled)])
if test "${enable_bonjour}" != "no"
then
  PKG_CHECK_MODULES(BONJOUR, avahi-client >= 0.3,
    [PKG_CHECK_MODULES(BONJOUR, avahi-client >= 0.6,
       [AC_DEFINE(HAVE_AVAHI_06, 1, [Define if you have avahi-client 0.6 or greater])],)
      AC_DEFINE(HAVE_AVAHI_CLIENT, 1, [Define if you have the avahi-client library])
      VLC_ADD_LIBS([bonjour access_output_http],[$BONJOUR_LIBS])
      VLC_ADD_CFLAGS([bonjour access_output_http],[$BONJOUR_CFLAGS])
      VLC_ADD_PLUGINS([bonjour]) ],
    [AC_MSG_WARN(avahi-client library not found)])
fi

dnl
dnl  Lirc plugin
dnl
AC_ARG_ENABLE(lirc,
  [  --enable-lirc           lirc support (default disabled)])
if test "${enable_lirc}" = "yes"
then
  AC_CHECK_HEADER(lirc/lirc_client.h, AC_CHECK_LIB(lirc_client, lirc_init, have_lirc="true", have_lirc="false"),have_lirc="false")
  if test "${have_lirc}" = "true"
  then
    VLC_ADD_PLUGINS([lirc])
    VLC_ADD_LIBS([lirc],[-llirc_client])
  fi
fi


AC_ARG_WITH(,[Misc options:])

dnl
dnl libgcrypt
dnl
AC_ARG_ENABLE(libgcrypt,
  [  --enable-libgcrypt      libgcrypts support (default enabled)])

GCRYPT_PATH="${PATH}"
AC_ARG_WITH(libgcrypt-config-path,
[    --with-libgcrypt-config-path=PATH libgcrypt-config path (default search in \$PATH)],
   [ if test "${with_libgcrypt_config_path}" != "no"
     then
       GCRYPT_PATH="${with_libgcrypt_config_path}:${PATH}"
     fi ])

if test "${enable_libgcrypt}" != "no"
then
  dnl Workaround for cross-compiling since AM_PATH_LIBGRYPT doesn't dectect
  dnl it and looks for libgcrypt headers in the wrong place.
  if test "${host_cpu}" = "${build_cpu}"
  then
    AM_PATH_LIBGCRYPT([1:1.1.94], [have_libgcrypt="yes"], [have_libgcrypt="no"])
  else
    AC_CHECK_HEADERS(gcrypt.h,have_libgcrypt="yes",have_libgcrypt="no")
  fi
fi
AM_CONDITIONAL([HAVE_LIBGCRYPT], [test "${have_libgcrypt}" = "yes"])
AS_IF([test "${have_libgcrypt}" = "yes"],[
	# look for libgcrypt-config
	AC_PATH_PROG(GCRYPT_CONFIG, "libgcrypt-config", no, ${GCRYPT_PATH})
	GCRYPT_CFLAGS=`${GCRYPT_CONFIG} --cflags`
	GCRYPT_LIBS=`${GCRYPT_CONFIG} --libs`
])

dnl
dnl TLS/SSL
dnl
AC_ARG_ENABLE(gnutls,
  [  --enable-gnutls         gnutls TLS/SSL support (default enabled)])

AS_IF([test "${enable_gnutls}" != "no"], [
  PKG_CHECK_MODULES(GNUTLS, [gnutls >= 1.3.3], [
    VLC_ADD_PLUGINS([gnutls])
    VLC_ADD_CFLAGS([gnutls], [$GNUTLS_CFLAGS])
    AS_IF([test "${SYS}" = "mingw32"], [
      dnl pkg-config --libs gnutls omits these
      VLC_ADD_LIBS([gnutls], [-lz])
      VLC_ADD_LIBS([gnutls], [${LTLIBINTL}])
    ])
    AS_IF([test "${have_libgcrypt}" = "yes"],[
      VLC_ADD_LIBS([gnutls], ${GCRYPT_LIBS})
      VLC_ADD_CFLAGS([gnutls], ${GCRYPT_CFLAGS})
    ], [-lgpg-error])
    VLC_ADD_LIBS([gnutls], [$GNUTLS_LIBS])
  ], [
    AS_IF([test "${enable_gnutls}" = "yes"], [
      AC_MSG_ERROR([gnutls not present or too old (version 1.2.9 required)])
    ])
  ])
])

dnl
dnl update checking system
dnl
AC_ARG_ENABLE(update-check,
  [  --enable-update-check   update checking system (default disabled)])
if test "${enable_update_check}" = "yes"
then
  if test "${have_libgcrypt}" != "yes"
  then
    AC_MSG_ERROR([libgcrypt is required for update checking system])
  fi
  VLC_ADD_LIBS([libvlc], ${GCRYPT_LIBS})
  VLC_ADD_CFLAGS([libvlc], ${GCRYPT_CFLAGS})
  AC_DEFINE([UPDATE_CHECK], 1, [Define if you want to use the VLC update mechanism])
fi

dnl
dnl  Endianness check, AC_C_BIGENDIAN doesn't work if we are cross-compiling
dnl
dnl  We give the user the opportunity to specify
dnl  --with-words=big or --with-words=little ; otherwise, try to guess
dnl
AC_ARG_WITH(words,
  [    --with-words=endianness set endianness (big or little)])
  case "${with_words}" in
    big)
      ac_cv_c_bigendian=yes
      ;;
    little)
      ac_cv_c_bigendian=no
      ;;
    *)
      dnl  Try to guess endianness by matching patterns on a compiled
      dnl  binary, by looking for an ASCII or EBCDIC string
      AC_CACHE_CHECK([whether the byte order is big-endian],
        [ac_cv_c_bigendian],
        [ac_cv_c_bigendian="unknown"
        [cat >conftest.c <<EOF
        short am[] = { 0x4249, 0x4765, 0x6e44, 0x6961, 0x6e53, 0x7953, 0 };
        short ai[] = { 0x694c, 0x5454, 0x656c, 0x6e45, 0x6944, 0x6e61, 0 };
        void _a(void) { char*s = (char*)am; s = (char *)ai; }
        short ei[] = { 0x89D3, 0xe3e3, 0x8593, 0x95c5, 0x89c4, 0x9581, 0 };
        short em[] = { 0xc2c9, 0xc785, 0x95c4, 0x8981, 0x95e2, 0xa8e2, 0 };
        void _e(void) { char*s = (char*)em; s = (char*)ei; }
        int main(void) { _a(); _e(); return 0; }
EOF
        ]
        if test -f conftest.c
        then
          if ${CC-cc} -c conftest.c -o conftest.o >>config.log 2>&1 \
              && test -f conftest.o
          then
            if test "`strings conftest.o | grep BIGenDianSyS`"
            then
              ac_cv_c_bigendian="yes"
            fi
            if test "`strings conftest.o | grep LiTTleEnDian`"
            then
              ac_cv_c_bigendian="no"
            fi
          fi
        fi
      ])
      if test "${ac_cv_c_bigendian}" = "unknown"
      then
        AC_MSG_ERROR([Could not guess endianness, please use --with-words])
      fi
      ;;
  esac
dnl  Now we know what to use for endianness, just put it in the header
if test "${ac_cv_c_bigendian}" = "yes"
then
  AC_DEFINE(WORDS_BIGENDIAN, 1, big endian system)
fi

dnl
dnl  DLL loader copied from MPlayer copied from somewhere else (WINE ?)
dnl
loader=false
AC_ARG_ENABLE(loader,
  [  --enable-loader         build DLL loader for ELF i386 platforms (default disabled)])
AM_CONDITIONAL(LOADER, [test "${enable_loader}" = "yes"])
AS_IF([test "${enable_loader}" = "yes"],
  [ VLC_ADD_PLUGINS([dmo])
    VLC_ADD_CPPFLAGS([dmo],[-I../../../@top_srcdir@/libs/loader])
    VLC_ADD_LIBS([dmo],[../../../libs/loader/libloader.la -lpthread])
    VLC_ADD_CPPFLAGS([quicktime],[-I../../@top_srcdir@/libs/loader])
    VLC_ADD_LIBS([quicktime],[../../libs/loader/libloader.la -lpthread])
    VLC_ADD_CPPFLAGS([realaudio],[-I../../@top_srcdir@/libs/loader -DLOADER])
    VLC_ADD_LIBS([realaudio],[../../libs/loader/libloader.la])
  ])

dnl
dnl  Microsoft ActiveX support
dnl
activex=false
AC_ARG_ENABLE(activex,
  [  --enable-activex        build a vlc-based ActiveX control (default enabled on Win32)])
AC_ARG_WITH(wine-sdk-path,
  [    --with-wine-sdk-path=PATH path to wine sdk])
if test "${enable_activex}" != "no"
then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"
  then
    AC_CHECK_PROGS(MIDL, [midl], no)
    if test "${with_wine_sdk_path}" != ""
    then
       WINE_SDK_PATH=${with_wine_sdk_path}
       AC_PATH_PROG(WIDL, widl, no, [$WINE_SDK_PATH/bin:$WINE_SDK_PATH/tools/widl])
    else
       WIDL=no
    fi
    AC_LANG_PUSH(C++)
    AC_CHECK_HEADERS(ole2.h, 
      [AC_CHECK_HEADERS(olectl.h,
        [ VLC_ADD_CPPFLAGS([activex],[-DUNICODE -D_UNICODE -D_MIDL_USE_GUIDDEF_])
          VLC_ADD_CXXFLAGS([activex],[-fno-exceptions])
          VLC_ADD_LIBS([activex],[-lole32 -loleaut32 -luuid -lshlwapi]) 
          AC_CHECK_HEADERS(objsafe.h,
            VLC_ADD_CXXFLAGS([activex],[-DHAVE_OBJSAFE_HEADER]),,
            [
	     #if HAVE_OLE2_H
             #   include <ole2.h>
             #endif
	     ]
          )
          activex=:
  	  PLUGINS_BINDINGS="${PLUGINS_BINDINGS} activex"
        ],
        [ AC_MSG_ERROR([required OLE headers are missing from your system]) ]
      )],
      [ AC_MSG_ERROR([required OLE headers are missing from your system]) ]
    )
    AC_LANG_POP(C++)
  fi
fi
AC_ARG_VAR(MIDL, [Microsoft IDL compiler (Win32 platform only)])
AM_CONDITIONAL(HAS_MIDL_COMPILER, test "${MIDL}" != "no")
AC_ARG_VAR(WIDL, [Wine IDL compiler (requires Wine SDK)])
AM_CONDITIONAL(HAS_WIDL_COMPILER, test "${WIDL}" != "no")
AM_CONDITIONAL(BUILD_ACTIVEX,${activex})

dnl
dnl  Mozilla plugin
dnl
mozilla=false
AC_ARG_ENABLE(mozilla,
  [  --enable-mozilla        build a vlc-based Firefox/Mozilla plugin (default disabled)])
AC_ARG_WITH(mozilla-sdk-path,
  [    --with-mozilla-sdk-path=PATH path to mozilla sdk])
AC_LANG_PUSH(C++)
if test "${enable_mozilla}" = "yes" 
then
  dnl currently vlc plugin only needs headers, no xpcom support apis are actually used
  need_xpcom_libs=false

  if test "${with_mozilla_sdk_path}" = "" -o "${with_mozilla_sdk_path}" = "no"
  then
    dnl New firefox-1.5 SDK api
    PKG_CHECK_MODULES([FIREFOX], [firefox-plugin],
      [
        PKG_CHECK_MODULES([FIREFOX_XPCOM], [firefox-xpcom],
          [],
          [
            PKG_CHECK_MODULES([MOZILLA_NSPR], [mozilla-nspr],
              [],
              [
                PKG_CHECK_MODULES([NSPR], [nspr],
                  [],
                  [
                    AC_MSG_ERROR([Please install the nspr development tools.])
                  ]
                )
              ]
            )
          ])
        CPPFLAGS="${CPPFLAGS_save} ${FIREFOX_CFLAGS} ${FIREFOX_XPCOM_CFLAGS} ${MOZILLA_NSPR_CFLAGS} ${NSPR_CFLAGS} ${MOZILLA_CPPFLAGS}"
        MOZILLA_REQUIRED_HEADERS=1
        AC_CHECK_HEADERS(mozilla-config.h,,MOZILLA_REQUIRED_HEADERS=0)
        AC_CHECK_HEADERS(npapi.h,,MOZILLA_REQUIRED_HEADERS=0)
        AC_CHECK_HEADERS(npruntime.h,,MOZILLA_REQUIRED_HEADERS=0,
          [#if HAVE_NPAPI_H
           # include <npapi.h>
           #endif
          ])
        if test "${MOZILLA_REQUIRED_HEADERS}" = "0"; then
            AC_MSG_ERROR([Please install the Firefox development tools; mozilla-config.h, plugin/npapi.h and plugin/npruntime.h were not found.])
        fi
        MOZILLA_REQUIRED_HEADERS=
        mozilla=:
        firefox_include=`pkg-config --variable=includedir firefox-plugin`
        if test -f "${firefox_include}/mozilla-config.h" -a -f "${firefox_include}/mozilla-config32.h"; then
            MOZILLA_CONFIG_H="${firefox_include}/mozilla-config32.h"
        elif test -f "${firefox_include}/mozilla-config.h" -a -f "${firefox_include}/mozilla-config64.h"; then
            MOZILLA_CONFIG_H="${firefox_include}/mozilla-config64.h"
        else
            MOZILLA_CONFIG_H="${firefox_include}/mozilla-config.h"
        fi
        if grep '^#define MOZ_X11 1' ${MOZILLA_CONFIG_H} 2>&1 > /dev/null ; then
            AC_CHECK_HEADERS(X11/xpm.h,,AC_MSG_ERROR([Please install libXpm-devel library for required X11/xpm.h]))
            VLC_ADD_LIBS([mozilla], [${X_LIBS} ${X_PRE_LIBS} -lX11 -lXt -lXpm])
        fi
        VLC_ADD_CPPFLAGS([mozilla],[${CPPFLAGS}])
        VLC_ADD_LDFLAGS([mozilla],[${MOZILLA_LDFLAGS}])
        VLC_ADD_LIBS([mozilla],[${FIREFOX_LIBS} ${MOZILLA_NSPR_LIBS} ${NSPR_LIBS}])
        VLC_ADD_PLUGINS([mozilla])
        MOZILLA_CONFIG=
        CPPFLAGS="${CPPFLAGS_save}"
      ],
      [
        AC_PATH_PROG(MOZILLA_CONFIG, mozilla-config, no)
        if test "${MOZILLA_CONFIG}" = "no"
        then
          AC_PATH_PROG(SEAMONKEY_CONFIG, seamonkey-config, no)
          if test "${SEAMONKEY_CONFIG}" = "no"
          then
            AC_PATH_PROG(XULRUNNER_CONFIG, xulrunner-config, no)
            if test "${XULRUNNER_CONFIG}" = "no"
            then
              AC_MSG_ERROR([Please install the Mozilla development tools, mozilla-config was not found.])
            else
              have_xul=true
              MOZILLA_CONFIG="${XULRUNNER_CONFIG}"
            fi
          else
            MOZILLA_CONFIG="${SEAMONKEY_CONFIG}"
          fi
        fi
      ]
    )
    if test ! -z "${MOZILLA_CONFIG}"
    then
      if ${MOZILLA_CONFIG} --defines | grep -q 'MOZ_X11=1'; then
        LDFLAGS="${LDFLAGS_save} ${X_LIBS} ${X_PRE_LIBS}"
        AC_CHECK_LIB(Xt,XtStrings,
         [
           VLC_ADD_CPPFLAGS([mozilla],[${X_CFLAGS}])
           VLC_ADD_LIBS([mozilla],[${X_LIBS} ${X_PRE_LIBS} -lXt -lX11 -lSM -lICE -lXpm])
         ],
         [],
         [[${X_LIBS} ${X_PRE_LIBS} -lX11 -lSM -lICE -lXpm]
        ])
        AC_CHECK_HEADERS(X11/xpm.h,,AC_MSG_ERROR([Please install libXpm-devel library for required X11/xpm.h]))
        LDFLAGS="${LDFLAGS_save}"
      fi

      mozilla=:
      PLUGINS_BINDINGS="${PLUGINS_BINDINGS} mozilla"
      dnl Workaround for http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=150490
      VLC_ADD_CPPFLAGS([mozilla],[[`${MOZILLA_CONFIG} --cflags plugin xpcom java | sed 's,-I\([^ ]*\)/mozilla/\([^ ]*\),-I\1/\2 -I\1/mozilla/\2,g' | xargs`]])
      if ${need_xpcom_libs}; then
         VLC_ADD_LIBS([mozilla],[`${MOZILLA_CONFIG} --libs plugin xpcom`])
      else
         VLC_ADD_LIBS([mozilla],[`${MOZILLA_CONFIG} --libs plugin`])
      fi
      CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_mozilla}"
      MOZILLA_REQUIRED_HEADERS=1
      AC_CHECK_HEADERS(mozilla-config.h,,MOZILLA_REQUIRED_HEADERS=0)
      AC_CHECK_HEADERS(npapi.h,,MOZILLA_REQUIRED_HEADERS=0)
      AC_CHECK_HEADERS(npruntime.h,,MOZILLA_REQUIRED_HEADERS=0,
        [#if HAVE_NPAPI_H
         # include <npapi.h>
         #endif
        ])
      if test "${MOZILLA_REQUIRED_HEADERS}" = "0"
      then
        AC_MSG_ERROR([Please install the Mozilla development tools, required headers were not found.])
      fi
      MOZILLA_REQUIRED_HEADERS=
      CPPFLAGS="${CPPFLAGS_save}"
      MOZILLA_SDK_PATH="`${MOZILLA_CONFIG} --prefix`"
    fi
    dnl End of moz_sdk = ""
  else
    dnl special case for mingw32
    if test "${SYS}" = "mingw32"
    then
      AC_CHECK_TOOL(CYGPATH, cygpath, "")
      dnl latest gecko sdk does not have an xpcom directory
      if test -d "${with_mozilla_sdk_path}/xpcom"; then
          mozilla_sdk_xpcom="/xpcom"
      fi
    fi

    real_mozilla_sdk="`cd ${with_mozilla_sdk_path} 2>/dev/null && pwd`"
    CPPFLAGS="${CPPFLAGS_save} -I${real_mozilla_sdk} -I${real_mozilla_sdk}/include"
    MOZILLA_REQUIRED_HEADERS=1
    AC_CHECK_HEADERS(mozilla-config.h,,MOZILLA_REQUIRED_HEADERS=0)
    AC_CHECK_HEADERS(npapi.h,,MOZILLA_REQUIRED_HEADERS=0)
    AC_CHECK_HEADERS(npruntime.h,,MOZILLA_REQUIRED_HEADERS=0,
        [#if HAVE_NPAPI_H
          #include <npapi.h>
          #endif
        ])
    if test "${MOZILLA_REQUIRED_HEADERS}" = "0"
    then
        AC_MSG_ERROR([Please install the Mozilla development tools, required headers were not found.])
    fi
    MOZILLA_REQUIRED_HEADERS=
    mozilla=:
    PLUGINS_BINDINGS="${PLUGINS_BINDINGS} mozilla"
    VLC_ADD_CPPFLAGS([mozilla],[-DXPCOM_GLUE -DHAVE_CPP_MODERN_SPECIALIZE_TEMPLATE_SYNTAX -I${real_mozilla_sdk} -I${real_mozilla_sdk}/include -I${real_mozilla_sdk}/embedstring/include -I${real_mozilla_sdk}/xpcom/include -I${real_mozilla_sdk}/nspr/include -I${real_mozilla_sdk}/string/include -I${real_mozilla_sdk}/plugin/include -I${real_mozilla_sdk}/java/include])
    if ${need_xpcom_libs}; then
       VLC_ADD_LIBS([mozilla],[-L${real_mozilla_sdk}/embedstring/bin -L${real_mozilla_sdk}/xpcom/bin -L${real_mozilla_sdk}/nspr/bin -L${real_mozilla_sdk}/string/bin -L${real_mozilla_sdk}/lib -lnspr4 -lplds4 -lplc4 -lxpcomglue])
      if test "${SYS}" = "mingw32"; then
        dnl latest gecko sdk does not have embedstring
        if test -d "${real_mozilla_sdk}/embedstring/bin"
        then
          VLC_ADD_LIBS([mozilla],[-lembedstring])
        fi
      fi
    fi
    MOZILLA_SDK_PATH="${real_mozilla_sdk}"

    if test -n "${CYGPATH}"; then
      real_mozilla_sdk="`${CYGPATH} -w ${real_mozilla_sdk}`"
    fi
    CPPFLAGS="${CPPFLAGS_save}"
  fi
fi
AC_LANG_POP(C++)
AM_CONDITIONAL(BUILD_MOZILLA,${mozilla})

dnl Tests for Osso and Xsp
if test "${enable_x11}" != "no" &&
  (test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" ||
   test "${enable_x11}" = "yes"); then
   AC_CHECK_LIB(osso, osso_display_blanking_pause,[
      PKG_CHECK_MODULES(GLIB2, glib-2.0, [						  
          VLC_ADD_CPPFLAGS([x11],[-DHAVE_OSSO ${DBUS_CFLAGS} ${GLIB2_CFLAGS}])
          VLC_ADD_LIBS([x11],[-losso])
      ])
   ])
   AC_CHECK_LIB(Xsp, XSPSetPixelDoubling,[
      VLC_ADD_CPPFLAGS([x11],[-DHAVE_XSP])
      VLC_ADD_LIBS([x11],[-lXsp])
   ])
fi

dnl
dnl  Mediacontrol Python bindings
dnl
AC_ARG_ENABLE(python-bindings,
  [  --enable-python-bindings Enable Python bindings (default disabled)])
dnl TODO: look for python dev headers
AS_IF([test "${enable_python_bindings}" = "yes"],
      PLUGINS_BINDINGS="${PLUGINS_BINDINGS} python"
     ) 

AM_CONDITIONAL(BUILD_PYTHON, [test "${enable_python_bindings}" = "yes"])

dnl
dnl  Java bindings
dnl
AC_ARG_ENABLE(java-bindings,
  [  --enable-java-bindings  Enable Java bindings (default disabled)])
if test "${enable_java_bindings}" = "yes"
then
  AS_IF([test "x${JAVA_HOME}" == "x"],
	[AC_MSG_ERROR([Please set the JAVA_HOME variable to your JDK environment])])
  AC_PROG_JAVAC
  AC_PROG_JAVA
  PLUGINS_BINDINGS="${PLUGINS_BINDINGS} java"
  if test "${JAVAC}" == "javac${EXEEXT}"
  then
    JAVAC="javac${EXEEXT} -cp ./ "
  fi
fi
AM_CONDITIONAL(BUILD_JAVA, [test "${enable_java_bindings}" = "yes"])

dnl
dnl  test plugins
dnl
AC_ARG_ENABLE(testsuite,
  [  --enable-testsuite      build test modules (default disabled)])
if test "${enable_testsuite}" = "yes"
then
  TESTS="test1 test2 test3 test4"

  dnl  we define those so that bootstrap sets the right linker
  VLC_ADD_CXXFLAGS([test2],[])
  VLC_ADD_OBJCFLAGS([test3],[])
  dnl  this one is needed until automake knows what to do
  VLC_ADD_LIBS([test3],[-lobjc])

  VLC_ADD_PLUGINS([${TESTS}])
fi

dnl
dnl Microsoft Layer for Unicode for WIN9x/ME
dnl
if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"
then
    AC_CHECK_LIB(unicows, LoadUnicowsSymbol, [
        VLC_ADD_LDFLAGS([vlc libvlc],[-Wl,--exclude-libs,libunicows.a -lunicows])
    ])
fi

dnl
dnl  gtk_main plugin
dnl
if test "${NEED_GTK_MAIN}" != "no"
then
    VLC_ADD_PLUGINS([gtk_main])
    VLC_ADD_CFLAGS([gtk_main],[${CFLAGS_gtk}])
    VLC_ADD_LDFLAGS([gtk_main],[${LDFLAGS_gtk}])
fi

if test "${NEED_GNOME_MAIN}" != "no"
then
    VLC_ADD_PLUGINS([gnome_main])
    VLC_ADD_CFLAGS([gnome_main],[${CFLAGS_gtk} ${CFLAGS_gnome}])
    VLC_ADD_LDFLAGS([gnome_main],[${LDFLAGS_gtk} ${LDFLAGS_gnome}])
fi

if test "${NEED_GTK2_MAIN}" != "no"
then
    VLC_ADD_PLUGINS([gtk2_main])
    VLC_ADD_CFLAGS([gtk2],[-DNEED_GTK2_MAIN])
    VLC_ADD_CFLAGS([pda],[-DNEED_GTK2_MAIN])
    VLC_ADD_CFLAGS([gtk2_main],[${CFLAGS_gtk2} ${CFLAGS_pda}])
    VLC_ADD_LDFLAGS([gtk2_main],[${LDFLAGS_gtk2} ${LDFLAGS_pda}])
fi

if test "${NEED_GNOME2_MAIN}" != "no"
then
    VLC_ADD_PLUGINS([gnome2_main])
    VLC_ADD_CFLAGS([gnome2_main],[${CFLAGS_gtk2} ${CFLAGS_gnome2}])
    VLC_ADD_LDFLAGS([gnome2_main],[${LDFLAGS_gtk2} ${LDFLAGS_gnome2}])
fi

dnl
dnl  qte_main plugin
dnl
if test "${NEED_QTE_MAIN}" != "no"
then
    VLC_ADD_PLUGINS([qte_main])
    VLC_ADD_CXXFLAGS([opie qte qt_video],[-DNEED_QTE_MAIN])
    VLC_ADD_CXXFLAGS([qte_main],[${CXXFLAGS_qte} ${CXXFLAGS_qt_video}])
    VLC_ADD_LDFLAGS([qte_main],[${LDFLAGS_qte} ${LDFLAGS_qt_video}])
fi


dnl
dnl  Plugin and builtin checks
dnl
plugin_support=yes

dnl Automagically disable plugins if there is no system support for
dnl dynamically loadable files (.so, .dll, .dylib).
dnl don't forget vlc-win32 still can load .dll as plugins
AS_IF([test "${ac_cv_have_plugins}" = "no"], [
  AC_MSG_WARN([*** No plugin support! Building statically! ***])
  plugin_support=no
])

AS_IF([test "${enable_shared}" = "no"], [
  plugin_support=no
])

AS_IF([test "${plugin_support}" != "no"], [
  AC_DEFINE(HAVE_DYNAMIC_PLUGINS, 1, Define if we have support for dynamic plugins)
])

AM_CONDITIONAL(HAVE_PLUGINS, [test "${plugin_support}" != "no"])

dnl Temporary(?) hack for plugins/builtins transition
VLC_ADD_PLUGINS([${BUILTINS}])
BUILTINS=""

dnl
dnl Pic and shared libvlc stuff
dnl
AS_IF([test "${SYS}" = "mingw32"], [
  FILE_LIBVLC_DLL="!define LIBVLC_DLL libvlc.dll"
  FILE_LIBVLC_CONTROL_DLL="!define LIBVLC_CONTROL_DLL libvlc-control.dll"
])

dnl
dnl  Stuff used by the program
dnl
AC_DEFINE_UNQUOTED(VERSION_MESSAGE, "${VERSION} ${CODENAME}", [Simple version string])
AC_DEFINE_UNQUOTED(COPYRIGHT_MESSAGE, "VLC media player - version ${VERSION} ${CODENAME} - (c) ${COPYRIGHT_YEARS} the VideoLAN team", [Copyright string])
AC_DEFINE_UNQUOTED(COPYRIGHT_YEARS, "${COPYRIGHT_YEARS}", [The copyright years])
AC_DEFINE_UNQUOTED(CONFIGURE_LINE, "${CONFIGURE_LINE}", [The ./configure command line])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_MAJOR,"${VERSION_MAJOR}", [version major number])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_MINOR,"${VERSION_MINOR}", [version minor number])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_REVISION,"${VERSION_REVISION}", [version minor number])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_EXTRA,"${VERSION_EXTRA}", [version minor number])
AC_SUBST(VERSION_MAJOR)
AC_SUBST(VERSION_MINOR)
AC_SUBST(VERSION_REVISION)
AC_DEFINE_UNQUOTED(VLC_COMPILE_BY, "`whoami`", [user who ran configure]) 
if test "${build_os}" = "cygwin"
then
    AC_DEFINE_UNQUOTED(VLC_COMPILE_HOST, "`hostname`", [host which ran configure])
else
    AC_DEFINE_UNQUOTED(VLC_COMPILE_HOST, "`hostname -s`", [host which ran configure])
fi
AC_DEFINE_UNQUOTED(VLC_COMPILE_DOMAIN, "`dnsdomainname 2>/dev/null || domainname 2>/dev/null || echo unknown`", [domain of the host which ran configure]) 
AC_DEFINE_UNQUOTED(VLC_COMPILER, "`$CC -v 2>&1 | tail -n 1`", [compiler]) 

dnl
dnl  Handle substvars that use $(top_srcdir)
dnl
VLC_CONFIG="top_builddir=\"\$(top_builddir)\" \$(top_builddir)/vlc-config"
AC_SUBST(VLC_CONFIG)
CPPFLAGS_save="${CPPFLAGS_save} -I\$(top_srcdir)/include -I\$(top_builddir)/include"

dnl
dnl  Restore *FLAGS
dnl
VLC_RESTORE_FLAGS

dnl
dnl  Create the vlc-config script
dnl
LDFLAGS_libvlc="${LDFLAGS_libvlc} ${LDFLAGS_builtin}"

dnl
dnl  Configuration is finished
dnl
AC_SUBST(SYS)
AC_SUBST(ARCH)
AC_SUBST(ALIASES)
AC_SUBST(ASM)
AC_SUBST(MOC)
AC_SUBST(RCC)
AC_SUBST(UIC)
AC_SUBST(WINDRES)
AC_SUBST(MOZILLA_SDK_PATH)
AC_SUBST(WINE_SDK_PATH)
AC_SUBST(LIBEXT)
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(MACOSX_DEPLOYMENT_TARGET)
AC_SUBST(FILE_LIBVLC_DLL)
AC_SUBST(FILE_LIBVLC_CONTROL_DLL)

dnl Create vlc-config.in
VLC_OUTPUT_VLC_CONFIG_IN

AC_CONFIG_FILES([
  extras/package/win32/vlc.win32.nsi
  Makefile
  projects/activex/Makefile
  projects/activex/axvlc.inf
  bindings/Makefile
  bindings/java/Makefile
  bindings/java/src/Makefile
  bindings/python/Makefile
  doc/Makefile
  extras/package/ipkg/Makefile
  libs/loader/Makefile
  libs/srtp/Makefile
  modules/Makefile
  projects/mozilla/Makefile
  m4/Makefile
  po/Makefile.in
  share/Makefile
  src/Makefile
])

AC_CONFIG_FILES([
  modules/access/Makefile
  modules/access/bda/Makefile
  modules/access/dshow/Makefile
  modules/access/dvb/Makefile
  modules/access/mms/Makefile
  modules/access/cdda/Makefile
  modules/access/rtsp/Makefile
  modules/access/v4l2/Makefile
  modules/access/vcd/Makefile
  modules/access/vcdx/Makefile
  modules/access/screen/Makefile
  modules/access_filter/Makefile
  modules/access_output/Makefile
  modules/audio_filter/Makefile
  modules/audio_filter/channel_mixer/Makefile
  modules/audio_filter/converter/Makefile
  modules/audio_filter/resampler/Makefile
  modules/audio_filter/spatializer/Makefile
  modules/audio_mixer/Makefile
  modules/audio_output/Makefile
  modules/codec/Makefile
  modules/codec/cmml/Makefile
  modules/codec/dmo/Makefile
  modules/codec/ffmpeg/Makefile
  modules/codec/subtitles/Makefile
  modules/codec/spudec/Makefile
  modules/codec/xvmc/Makefile
  modules/control/Makefile
  modules/control/http/Makefile
  modules/demux/Makefile
  modules/demux/asf/Makefile
  modules/demux/avi/Makefile
  modules/demux/mp4/Makefile
  modules/demux/mpeg/Makefile
  modules/demux/playlist/Makefile
  modules/gui/Makefile
  modules/gui/beos/Makefile
  modules/gui/pda/Makefile
  modules/gui/macosx/Makefile
  modules/gui/minimal_macosx/Makefile
  modules/gui/qnx/Makefile
  modules/gui/qt4/Makefile
  modules/gui/skins2/Makefile
  modules/gui/wxwidgets/Makefile
  modules/gui/wince/Makefile
  modules/meta_engine/Makefile
  modules/misc/Makefile
  modules/misc/dummy/Makefile
  modules/misc/lua/Makefile
  modules/misc/memcpy/Makefile
  modules/misc/notify/Makefile
  modules/misc/testsuite/Makefile
  modules/misc/playlist/Makefile
  modules/misc/osd/Makefile
  modules/misc/xml/Makefile
  modules/misc/probe/Makefile
  modules/mux/Makefile
  modules/mux/mpeg/Makefile
  modules/mux/rtp/Makefile
  modules/packetizer/Makefile
  modules/services_discovery/Makefile
  modules/stream_out/Makefile
  modules/stream_out/transrate/Makefile
  modules/video_chroma/Makefile
  modules/video_filter/Makefile
  modules/video_filter/atmo/Makefile
  modules/video_output/Makefile
  modules/video_output/msw/Makefile
  modules/video_output/qte/Makefile
  modules/video_output/x11/Makefile
  modules/visualization/Makefile
  modules/visualization/visual/Makefile
  modules/visualization/galaktos/Makefile
])

dnl Generate makefiles
AC_OUTPUT

# Cannot use AC_CONFIG_FILES([vlc-config]) as is automatically built,
# not provided with the source
${SHELL} ./config.status --file=vlc-config
chmod 0755 vlc-config

dnl echo "Enabled builtin modules :"
dnl for a in `./vlc-config --target builtin` ; do echo $a; done | sed -e 's,modules\/\(.*\)\/lib\(.*\)\.a,\2 (\1),'

dnl echo "Enabled plugin modules :"
dnl for a in `./vlc-config --target plugin` ; do echo $a; done | sed -e 's,modules\/\(.*\)\/lib\(.*\)_plugin,\2 (\1),'

dnl Shortcut to nice compile message
rm -f compile
echo '#! /bin/sh' >compile

echo "PATH=$PATH LANG=C make \$@ 2>&1| \\" >> compile
echo '`sed -ne "s/^top_srcdir *= *//p" < Makefile`/extras/buildsystem/make.pl' >> compile
chmod a+x compile

printf "
vlc configuration
--------------------
vlc version           : ${VERSION}
system                : ${SYS}
architecture          : ${ARCH}
build flavour         : "
test "${enable_debug}" = "yes" && printf "debug "
test "${enable_cprof}" = "yes" && printf "cprof "
test "${enable_gprof}" = "yes" && printf "gprof "
test "${enable_optimizations}" = "yes" && printf "optim "
test "${enable_release}" = "yes" && printf "release " || printf "devel "
echo "
vlc aliases           :${ALIASES}
plugins/bindings      :${PLUGINS_BINDINGS}

You can check which modules have been enabled 
with \`./vlc-config --list plugin'.
You can tune the compiler flags in vlc-config.
To build vlc and its plugins, type \`./compile' or \`make'.
"
if test "x$ac_ld_does_not_support_text_reloc" = "xyes"; then
   echo ""
   echo "Warning: Due to a bug in ld, mmx/sse support has been"
   echo "         turned off."
   echo "         FFmpeg will be REALLY slow."
   echo "         VLC WILL NOT PERFORM AS EXPECTED."
   echo ""
fi
