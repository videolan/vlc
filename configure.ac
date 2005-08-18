dnl Autoconf settings for vlc
dnl $Id$

AC_INIT(vlc,0.8.4-svn)
VERSION_MAJOR="0"
VERSION_MINOR="8"
VERSION_REVISION="4"
VERSION_EXTRA="svn"

CONFIGURE_LINE="$0 $*"
CODENAME="Janus"

AC_PREREQ(2.50)
AC_CONFIG_SRCDIR(src/libvlc.c)
AC_CONFIG_AUX_DIR(autotools)
AC_CANONICAL_SYSTEM

dnl XXX: we don't put any flags here, because automake 1.5 doesn't support
dnl them. And we need the comma otherwize automake will choke on it.
AM_INIT_AUTOMAKE(vlc,0.8.4-svn)
AM_CONFIG_HEADER(config.h)

dnl
dnl  Save *FLAGS
dnl
VLC_SAVE_FLAGS

dnl
dnl Check for tools
dnl
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_CXX
eval "${CXX} --version" >/dev/null 2>&1 || CXX=""
if test "${CXX}" != ""; then
  AC_PROG_CXXCPP
fi
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AM_PROG_AS

dnl AC_PROG_EGREP only exists in autoconf 2.54+, so we use AC_EGREP_CPP right
dnl now otherwise it might be set in an obscure if statement.
AC_EGREP_CPP(foo,foo)

dnl AC_PROG_OBJC doesn't seem to exist, this is the KDE workaround
AC_MSG_CHECKING(for an Objective-C compiler)
OBJC="${CXX}"
AC_SUBST(OBJC)
OBJCFLAGS="${CXXFLAGS} -fgnu-runtime -fconstant-string-class=NSConstantString"
AC_SUBST(OBJCFLAGS)
dnl _AM_DEPENDENCIES(OBJC) doesn't work, so hard code OBJCDEPMODE here
#OBJCDEPMODE="depmode=gcc3"
#AC_SUBST(OBJCDEPMODE)
_AM_DEPENDENCIES(OBJC)
AC_MSG_RESULT(not implemented yet)

dnl Find the right ranlib, even when cross-compiling
AC_CHECK_TOOL(RANLIB, ranlib, :)
AC_CHECK_TOOL(STRIP, strip, :)
AC_CHECK_TOOL(AR, ar, :)
AC_CHECK_TOOL(LD, ld, :)

dnl Sam, if you think I didn't see that... --Meuuh
dnl AM_PROG_LIBTOOL
AC_PROG_INSTALL

dnl Check for compiler properties
AC_C_CONST
AC_C_INLINE

dnl
dnl  Check for the contrib directory
dnl
topdir="`dirname $0`"
if test "`echo \"$topdir\" | cut -c 1`" != "/"; then
  topdir="`pwd`/$topdir"
fi

if test -d ${topdir}/extras/contrib/lib; then
  export PATH=${topdir}/extras/contrib/bin:$PATH
  CPPFLAGS="${CPPFLAGS} -I${topdir}/extras/contrib/include"
  CPPFLAGS_save="${CPPFLAGS_save} -I${topdir}/extras/contrib/include"
  CFLAGS="${CFLAGS} -I${topdir}/extras/contrib/include"
  CFLAGS_save="${CFLAGS_save} -I${topdir}/extras/contrib/include"
  CXXFLAGS="${CXXFLAGS} -I${topdir}/extras/contrib/include"
  CXXFLAGS_save="${CXXFLAGS_save} -I${topdir}/extras/contrib/include"
  OBJCFLAGS="${OBJCFLAGS} -I${topdir}/extras/contrib/include"
  OBJCFLAGS_save="${OBJCFLAGS_save} -I${topdir}/extras/contrib/include"
  if test -d ${topdir}/extras/contrib/vlc-lib; then
    LDFLAGS="${LDFLAGS} -L${topdir}/extras/contrib/vlc-lib"
    LDFLAGS_save="${LDFLAGS_save} -L${topdir}/extras/contrib/vlc-lib"
  fi
  LDFLAGS="${LDFLAGS} -L${topdir}/extras/contrib/lib"
  LDFLAGS_save="${LDFLAGS_save} -L${topdir}/extras/contrib/lib"
  with_livedotcom_tree=${topdir}/extras/contrib/src/live
  with_goom_tree=${topdir}/extras/contrib/src/goom
  if test ".`uname -s`" = ".Darwin"; then
    export LD_LIBRARY_PATH=${topdir}/extras/contrib/lib:$LD_LIBRARY_PATH
    export DYLD_LIBRARY_PATH=${topdir}/extras/contrib/lib:$DYLD_LIBRARY_PATH
  elif test ".`uname -s`" = ".BeOS"; then
    export LIBRARY_PATH=${topdir}/extras/contrib/lib:$LIBRARY_PATH
    export BELIBRARIES=${topdir}/extras/contrib/lib:$BELIBRARIES
  fi
fi

dnl
dnl  Set default values
dnl
LDFLAGS_vlc="${LDFLAGS}"

dnl
dnl  Check the operating system
dnl
case "${target_os}" in
  "")
    SYS=unknown
    ;;
  linux*)
    SYS=linux
    ;;
  bsdi*)
    SYS=bsdi
    CFLAGS_save="${CFLAGS_save} -pthread"; CFLAGS="${CFLAGS_save}"
    VLC_ADD_LDFLAGS([dvd dvdcss vcd cdda vcdx cddax],[-ldvd])
    ;;
  *bsd*)
    SYS="${target_os}"
    CFLAGS_save="${CFLAGS_save} -pthread"; CFLAGS="${CFLAGS_save}"
    ;;
  darwin*)
    SYS=darwin
    CFLAGS_save="${CFLAGS_save} -no-cpp-precomp -D_INTL_REDIRECT_MACROS"; CFLAGS="${CFLAGS_save}"
    CXXFLAGS_save="${CXXFLAGS_save} -no-cpp-precomp -D_INTL_REDIRECT_MACROS"; CXXFLAGS="${CXXFLAGS_save}"
    OBJCFLAGS_save="${OBJCFLAGS_save} -no-cpp-precomp -D_INTL_REDIRECT_MACROS"; OBJCFLAGS="${OBJCFLAGS_save}"
    VLC_ADD_LDFLAGS([vlc ffmpeg ffmpegaltivec],[-all_load])
    VLC_ADD_LDFLAGS([mp4], [-framework IOKit -framework CoreFoundation])
    VLC_ADD_CFLAGS([libvlc],[-x objective-c])
    VLC_ADD_CFLAGS([vlc],[-x objective-c])
    VLC_ADD_LDFLAGS([vlc],[-Wl,-m -Wl,-multiply_defined -Wl,suppress])
    ;;
  *mingw32* | *cygwin* | *wince* | *mingwce* | *pe*)
    AC_CHECK_TOOL(WINDRES, windres, :)

    case "${target_os}" in
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
        CPPFLAGS_save="${CPPFLAGS_save} -D_OFF_T_ -D_off_t=long"; CPPFLAGS="${CPPFLAGS_save}"
        VLC_ADD_LDFLAGS([vlc],[-lws2_32 -lnetapi32 -lwinmm -mwindows])
        VLC_ADD_LDFLAGS([vcdx cddax],[-lwinmm])
        VLC_ADD_LDFLAGS([ipv4 ipv6 access_http access_mms access_udp access_tcp access_ftp access_output_udp sap slp http stream_out_standard stream_out_rtp vod_rtsp telnet netsync],[-lws2_32])
        VLC_ADD_LDFLAGS([ipv4],[-liphlpapi])
    fi
    if test "${SYS}" = "mingwce"; then
        # add ws2 for closesocket, select, recv
        CPPFLAGS_save="${CPPFLAGS_save} -D_OFF_T_ -D_off_t=long"; CPPFLAGS="${CPPFLAGS_save}"
        VLC_ADD_CPPFLAGS([vlc],[-Dmain(a,b)=maince(a,b)])
        VLC_ADD_LDFLAGS([vlc],[-lws2 -e WinMainCRTStartup])
        VLC_ADD_LDFLAGS([ipv4 ipv6 access_http access_mms access_udp access_tcp access_ftp access_output_udp sap http netsync],[-lws2])
         VLC_ADD_LDFLAGS([ipv4],[-liphlpapi])
   fi
    ;;
  *nto*)
    SYS=nto
    VLC_ADD_LDFLAGS([x11 xvideo],[-lsocket])
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
    VLC_ADD_LDFLAGS([vlc beos],[-lbe])
    VLC_ADD_LDFLAGS([beos],[-lmedia -ltranslation -ltracker -lgame])
    VLC_ADD_LDFLAGS([access_file access_ftp access_mms access_output_udp telnet netsync sap ipv4 vlc],[-lnet])

    dnl Ugly check for Zeta
    if test -f /boot/beos/system/lib/libzeta.so; then
        VLC_ADD_LDFLAGS([beos],[-lzeta])
    fi
    ;;
  *)
    SYS="${target_os}"
    ;;
esac
AM_CONDITIONAL(HAVE_BEOS, test "${SYS}" = "beos")
AM_CONDITIONAL(HAVE_DARWIN, test "${SYS}" = "darwin")
AM_CONDITIONAL(HAVE_WIN32, test "${SYS}" = "mingw32")
AM_CONDITIONAL(HAVE_WINCE, test "${SYS}" = "mingwce")

dnl
dnl Gettext stuff
dnl
ALL_LINGUAS="ca da de en_GB es fr it ja nl pt_BR ro ru tr zh_TW"
AM_GNU_GETTEXT_VERSION(0.11.5)
AM_GNU_GETTEXT
if test "${nls_cv_force_use_gnu_gettext}" = "yes"; then
  AC_DEFINE(HAVE_INCLUDED_GETTEXT, 1, Define if we use the local libintl)
  INCLUDES="${INCLUDES} -I\$(top_builddir)/intl"
else
  AC_CHECK_FUNCS(textdomain,,[
    AC_CHECK_LIB(intl,textdomain,
      VLC_ADD_LDFLAGS([vlc],[${LIBINTL}]),,
      ${LIBINTL}
    )
  ])
fi
AM_CONDITIONAL(BUILD_INTL, test "${nls_cv_force_use_gnu_gettext}" = "yes")
XGETTEXT="${XGETTEXT} --keyword=_NS --keyword=_ANS"

dnl
dnl Iconv stuff
dnl
AS_IF([test "$am_cv_func_iconv" != "yes"],
	[AC_MSG_ERROR([libiconv is needed for VLC to work properly])])
VLC_ADD_CFLAGS([vlc],[${INCICONV}])
VLC_ADD_LDFLAGS([vlc],[${LIBICONV}])

dnl Check for the need to include the mingwex lib for mingw32
if test "${SYS}" = "mingw32"
then
    AC_CHECK_LIB(mingwex,opendir,
        AC_CHECK_LIB(mingw32,opendir,VLC_ADD_LDFLAGS([vlc],[]),
            [VLC_ADD_LDFLAGS([vlc gtk],[-lmingwex])])
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

case "${SYS}" in
  mingw32|cygwin)
    VLC_ADD_CFLAGS([pic plugin mozilla activex],[${CFLAGS_mingw32_special}])
    VLC_ADD_CXXFLAGS([pic plugin mozilla activex],[${CFLAGS_mingw32_special} ${CXXFLAGS_mingw32_special}])
    VLC_ADD_OBJCFLAGS([pic plugin mozilla activex],[${CFLAGS_mingw32_special}])
    ;;
  mingwce)
    ;;
  *)
    VLC_ADD_CFLAGS([pic plugin mozilla],[-fpic -fPIC])
    VLC_ADD_CXXFLAGS([pic plugin mozilla],[-fpic -fPIC])
    VLC_ADD_OBJCFLAGS([pic plugin mozilla],[-fpic -fPIC])
    VLC_ADD_LDFLAGS([plugin mozilla],[-fpic -fPIC])
    ;;
esac

dnl The -DSYS_FOO flag
CPPFLAGS_save="${CPPFLAGS_save} -DSYS_`echo ${SYS} | sed -e 's/-.*//' | tr 'abcdefghijklmnopqrstuvwxyz.' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ_'`"; CPPFLAGS="${CPPFLAGS_save}"

dnl Check for system libs needed
need_libc=false

AC_CHECK_FUNCS(gettimeofday select strerror strtod strtol strtof strtoll strtoull strsep isatty vasprintf asprintf swab sigrelse getpwuid memalign posix_memalign if_nametoindex atoll getenv putenv setenv gmtime_r ctime_r localtime_r lrintf daemon scandir)

dnl Check for usual libc functions
AC_CHECK_FUNCS(strdup strndup atof)
AC_CHECK_FUNCS(strcasecmp,,[AC_CHECK_FUNCS(stricmp)])
AC_CHECK_FUNCS(strncasecmp,,[AC_CHECK_FUNCS(strnicmp)])
AC_CHECK_FUNCS(strcasestr,,[AC_CHECK_FUNCS(stristr)])

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
    VLC_ADD_LDFLAGS([vlc ipv4 cddax],-lsocket)
  ])
])

AC_CHECK_FUNCS(send,,[
  AC_CHECK_LIB(socket,send,[
    VLC_ADD_LDFLAGS([access_http access_mms access_udp access_tcp access_ftp sap access_output_udp stream_out_standard],[-lsocket])
  ])
])

AC_CHECK_FUNCS(gethostbyname,,[
  AC_CHECK_LIB(nsl,gethostbyname,[
    VLC_ADD_LDFLAGS([cddax ipv4 vlc],[-lnsl])
  ],[
    AC_CHECK_LIB(bind,gethostbyname,[
      VLC_ADD_LDFLAGS([ipv4 access_mms],[-lbind])
    ])
  ])
])

dnl Check for socklen_t
AC_CACHE_CHECK([for socklen_t], ac_cv_type_socklen_t,
  [AC_TRY_COMPILE(
     [#include <sys/types.h>
      #include <sys/socket.h>],
     [socklen_t len = 42; return len;],
     ac_cv_type_socklen_t=yes,
     ac_cv_type_socklen_t=no)])
if test "${ac_cv_type_socklen_t}" != "no"; then
  AC_DEFINE(HAVE_SOCKLEN_T, 1, [Define if <sys/socket.h> defines socklen_t.])
fi

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
if test $ac_cv_struct_sockaddr_storage = no; then
  AC_DEFINE(sockaddr_storage, sockaddr)
  AC_DEFINE(ss_family, sa_family)
fi

dnl getaddrinfo, getnameinfo and gai_strerror check
dnl  -lresolv is NOT needed on Solaris
dnl  we purposedly make the test fail on Windows
AC_CHECK_FUNCS([getaddrinfo getnameinfo gai_strerror])
AH_TEMPLATE(HAVE_ADDRINFO, [Define to `1' if <netdb.h> defines struct addrinfo.])
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
    VLC_ADD_LDFLAGS([ipv4 vlc],[-lresolv])
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
     VLC_ADD_LDFLAGS([vlc],[-lgnugetopt])],
    [need_getopt=:])])
fi
AM_CONDITIONAL(BUILD_GETOPT, ${need_getopt})

if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
AC_TYPE_SIGNAL
AC_CHECK_LIB(m,cos,[
  VLC_ADD_LDFLAGS([adjust distort a52tofloat32 dtstofloat32 x264 goom],[-lm])
])
AC_CHECK_LIB(m,pow,[
  VLC_ADD_LDFLAGS([ffmpeg ffmpegaltivec stream_out_transrate i420_rgb faad twolame equalizer vlc freetype],[-lm])
])
AC_CHECK_LIB(m,sqrt,[
  VLC_ADD_LDFLAGS([headphone_channel_mixer normvol speex],[-lm])
])
AC_CHECK_LIB(mx,sqrtf,[
  VLC_ADD_LDFLAGS([x264],[-lmx])
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
      VLC_ADD_LDFLAGS([vlc],[-ldld])])])
  if test "${ac_cv_my_have_shl_load}" = "yes"; then
    AC_DEFINE(HAVE_DL_SHL_LOAD, 1, [Define if you have the shl_load API])
    ac_cv_have_plugins=yes
  fi
fi

# Whatever style
if test "${ac_cv_have_plugins}" = "no"; then
  AC_CHECK_LIB(dld, dld_link,
   [VLC_ADD_LDFLAGS([vlc],[-ldld])
    AC_DEFINE(HAVE_DL_DLD_LINK, 1, [Define if you have the GNU dld library])
    ac_cv_have_plugins=yes])
fi

# Win32 style
if test "${ac_cv_have_plugins}" = "no"; then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"; then
    AC_CHECK_LIB(kernel32, main,
     [VLC_ADD_LDFLAGS([vlc],[-lkernel32])
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
      VLC_ADD_LDFLAGS([vlc],[-ldl]),
      AC_CHECK_LIB(svld, dlopen,
        ac_cv_my_have_dlopen=yes
        VLC_ADD_LDFLAGS([vlc],[-lsvld]))))
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

dnl Check for cthreads under GNU/Hurd for instance
AC_CHECK_LIB(threads,cthread_fork,THREAD_LIB="-lthreads")

dnl
dnl  GNU portable threads
dnl
AC_ARG_ENABLE(pth,
  [  --enable-pth            GNU Pth support (default disabled)],
  [ if test "${enable_pth}" = "yes"; then
    AC_CHECK_LIB(pth,pth_init)
    AC_MSG_CHECKING(for pth_init in pth.h)
    AC_EGREP_HEADER(pth_init,pth.h,[
      AC_MSG_RESULT(yes)
      AC_DEFINE(PTH_INIT_IN_PTH_H, 1,
                Define if <pth.h> defines pth_init)
      THREAD_LIB="-lpth"
    ],[
      AC_MSG_RESULT(no)
    ])
    fi
])

dnl
dnl  State Threads
dnl
AC_ARG_ENABLE(st,
  [  --enable-st             State Threads (default disabled)],
  [ if test "${enable_st}" = "yes"; then
    AC_CHECK_LIB(st,st_init)
    AC_MSG_CHECKING(for st_init in st.h)
    AC_EGREP_HEADER(st_init,st.h,[
      AC_MSG_RESULT(yes)
      AC_DEFINE(ST_INIT_IN_ST_H, 1,
                Define if <st.h> defines st_init)
      THREAD_LIB="-lst"
    ],[
      AC_MSG_RESULT(yes)
    ])
    fi
])

VLC_ADD_LDFLAGS([vlc plugin],[${THREAD_LIB}])

dnl Don't link with rt when using GNU-pth
if test "${THREAD_LIB}" != "-lpth" && test "${THREAD_LIB}" != "-lst"; then
  dnl HP/UX port
  AC_CHECK_LIB(rt,sem_init, [VLC_ADD_LDFLAGS([vlc],[-lrt])])

  have_nanosleep=false
  AC_CHECK_FUNCS(nanosleep,have_nanosleep=:,[
    AC_CHECK_LIB(rt,nanosleep,
      [VLC_ADD_LDFLAGS([vlc],[-lrt]) have_nanosleep=:],
      [AC_CHECK_LIB(posix4,nanosleep,
          [VLC_ADD_LDFLAGS([vlc],[-lposix4]) have_nanosleep=:])]
    )
  ])
  if ${have_nanosleep}; then
    AC_DEFINE(HAVE_NANOSLEEP, 1,
              Define if nanosleep is available.)
  fi
fi

dnl Check for misc headers
AC_MSG_CHECKING(for pthread_cond_t in pthread.h)
AC_EGREP_HEADER(pthread_cond_t,pthread.h,[
  AC_MSG_RESULT(yes)
  AC_DEFINE(PTHREAD_COND_T_IN_PTHREAD_H, 1,
            Define if <pthread.h> defines pthread_cond_t.)],[
  AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for pthread_once in pthread.h)
AC_EGREP_HEADER(pthread_once,pthread.h,[
  AC_MSG_RESULT(yes)
  AC_DEFINE(PTHREAD_ONCE_IN_PTHREAD_H, 1,
            Define if <pthread.h> defines pthread_once.)],[
  AC_MSG_RESULT(no)])
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

AC_MSG_CHECKING(for strncasecmp in strings.h)
AC_EGREP_HEADER(strncasecmp,strings.h,[
  AC_MSG_RESULT(yes)
  AC_DEFINE(STRNCASECMP_IN_STRINGS_H, 1,
            Define if <strings.h> defines strncasecmp.)],[
  AC_MSG_RESULT(no)])

dnl Check for headers
AC_CHECK_HEADERS(signal.h time.h errno.h stdint.h stdbool.h getopt.h strings.h inttypes.h sys/int_types.h wchar.h)
AC_CHECK_HEADERS(sys/sockio.h fcntl.h sys/types.h sys/time.h sys/times.h sys/ioctl.h sys/stat.h)
AC_CHECK_HEADERS(arpa/inet.h net/if.h netinet/in.h sys/socket.h)
if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
AC_CHECK_HEADERS(machine/param.h sys/shm.h altivec.h)
AC_CHECK_HEADERS(linux/version.h)
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

AC_HEADER_TIME

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
AC_CHECK_HEADERS(cthreads.h pthread.h kernel/scheduler.h kernel/OS.h)
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

dnl Default X headers and libraries
if test "${x_includes}" = "NONE"; then
  x_includes="/usr/X11R6/include"
fi
if test "${x_libraries}" = "NONE"; then
  x_libraries="/usr/X11R6/lib"
fi

dnl Check for hal
AC_ARG_ENABLE(hal,
  [  --enable-hal            Linux HAL services discovery (default enabled)])
 
if test "${enable_hal}" != "no"
then
  PKG_CHECK_MODULES(HAL, hal >= 0.2.97,
    [AC_DEFINE(HAVE_HAL, [], [Define if you have the HAL library])
     VLC_ADD_PLUGINS([hal])
     VLC_ADD_LDFLAGS([vlc hal],[$HAL_LIBS])
     VLC_ADD_CFLAGS([vlc hal],[$HAL_CFLAGS])],
    [AC_MSG_WARN(HAL library not found)])
fi

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
        [void foo() { int meuh; ntohl(meuh); }],
        ac_cv_c_ntohl_sys_param_h=yes, ac_cv_c_ntohl_sys_param_h=no)])
if test "${ac_cv_c_ntohl_sys_param_h}" != "no"; then
    AC_DEFINE(NTOHL_IN_SYS_PARAM_H, 1, Define if <sys/param.h> defines ntohl.)
fi

# XXX: do this with an M4 macro?
#dnl Check for various -W flags
#for flag in "" all unreachable-code conversion sign-compare disabled-optimization
#do
#    AC_CACHE_CHECK([if \$CC accepts -W${flag}],
#        [ac_cv_c_W${flag}],
#        [CFLAGS="-W${flag} ${CFLAGS_save}"
#         AC_TRY_COMPILE([],,ac_cv_c_W${flag}=yes, ac_cv_c_W${flag}=no)])
#    if test "${ac_cv_c_W${flag}}" != "no"; then
#        CFLAGS_save="-W${flag} ${CFLAGS_save}"; CFLAGS="${CFLAGS_save}"
#        CXXFLAGS_save="-W${flag} ${CFLAGS_save}"; CXXFLAGS="${CXXFLAGS_save}"
#        OBJCFLAGS_save="-W${flag} ${OBJCFLAGS_save}"; OBJCFLAGS="${OBJCFLAGS_save}"
#    fi
#done

AC_CACHE_CHECK([if \$CC accepts -Wall],
    [ac_cv_c_Wall],
    [CFLAGS="-Wall ${CFLAGS_save}"
     AC_TRY_COMPILE([],,ac_cv_c_Wall=yes, ac_cv_c_Wall=no)])

AC_CACHE_CHECK([if \$CC accepts -Wconversion],
    [ac_cv_c_Wconversion],
    [CFLAGS="-Wconversion ${CFLAGS_save}"
     AC_TRY_COMPILE([],,ac_cv_c_Wconversion=yes, ac_cv_c_Wconversion=no)])

AC_CACHE_CHECK([if \$CC accepts -Wunreachable-code],
    [ac_cv_c_Wunreachable_code],
    [CFLAGS="-Wunreachable-code ${CFLAGS_save}"
     AC_TRY_COMPILE([],,ac_cv_c_Wunreachable_code=yes, ac_cv_c_Wunreachable_code=no)])

AC_CACHE_CHECK([if \$CC accepts -Wsign-compare],
    [ac_cv_c_Wsign_compare],
    [CFLAGS="-Wsign-compare ${CFLAGS_save}"
     AC_TRY_COMPILE([],,ac_cv_c_Wsign_compare=yes, ac_cv_c_Wsign_compare=no)])

if test "${ac_cv_c_Wall}" != "no"; then
    CFLAGS_save="-Wall ${CFLAGS_save}"; CFLAGS="${CFLAGS_save}"
    CXXFLAGS_save="-Wall ${CFLAGS_save}"; CXXFLAGS="${CXXFLAGS_save}"
    OBJCFLAGS_save="-Wall ${OBJCFLAGS_save}"; OBJCFLAGS="${OBJCFLAGS_save}"
fi

if test "${ac_cv_c_Wsign_compare}" != "no"; then
    CFLAGS_save="-Wsign-compare ${CFLAGS_save}"; CFLAGS="${CFLAGS_save}"
    CXXFLAGS_save="-Wsign-compare ${CFLAGS_save}"; CXXFLAGS="${CXXFLAGS_save}"
    OBJCFLAGS_save="-Wsign-compare ${OBJCFLAGS_save}"; OBJCFLAGS="${OBJCFLAGS_save}"
fi

dnl Check for -pipe
AC_CACHE_CHECK([if \$CC accepts -pipe],
    [ac_cv_c_pipe],
    [CFLAGS="${CFLAGS_save} -pipe"
     AC_TRY_COMPILE([],,ac_cv_c_pipe=yes, ac_cv_c_pipe=no)])
if test "${ac_cv_c_pipe}" != "no"; then
    CFLAGS_save="${CFLAGS_save} -pipe"; CFLAGS="${CFLAGS_save}"
    CXXFLAGS_save="${CXXFLAGS_save} -pipe"; CXXFLAGS="${CXXFLAGS_save}"
    OBJCFLAGS_save="${OBJCFLAGS_save} -pipe"; OBJCFLAGS="${OBJCFLAGS_save}"
fi

dnl Check for various optimization flags
AC_CACHE_CHECK([if \$CC accepts -Os],
    [ac_cv_c_os],
    [CFLAGS="${CFLAGS_save} -Os"
     AC_TRY_COMPILE([],,ac_cv_c_os=yes, ac_cv_c_os=no)])
if test "${ac_cv_c_os}" != "no" -a "${target_cpu}" = "mipsel"; then
    CFLAGS_OPTIM="${CFLAGS_OPTIM} -Os"
fi

AC_CACHE_CHECK([if \$CC accepts -O3],
    [ac_cv_c_o3],
    [CFLAGS="${CFLAGS_save} -O3"
     AC_TRY_COMPILE([],,ac_cv_c_o3=yes, ac_cv_c_o3=no)])
if test "${ac_cv_c_o3}" != "no" -a "${target_cpu}" != "mipsel"; then
    CFLAGS_OPTIM="${CFLAGS_OPTIM} -O3"
fi

AC_CACHE_CHECK([if \$CC accepts -O2],
    [ac_cv_c_o2],
    [CFLAGS="${CFLAGS_save} -O2"
     AC_TRY_COMPILE([],,ac_cv_c_o2=yes, ac_cv_c_o2=no)])
if test "${ac_cv_c_o2}" != "no" -a "${target_cpu}" != "mipsel"; then
    if test "${ac_cv_c_o3}" = "no"; then
        CFLAGS_OPTIM="${CFLAGS_OPTIM} -O2"
    fi
    CFLAGS_NOOPTIM="${CFLAGS_NOOPTIM} -O2"
else 
    AC_CACHE_CHECK([if \$CC accepts -O],
        [ac_cv_c_o],
        [CFLAGS="${CFLAGS_save} -O"
         AC_TRY_COMPILE([],,ac_cv_c_o=yes, ac_cv_c_o=no)])
    if test "${ac_cv_c_o}" != "no" -a "${target_cpu}" != "mipsel"; then
        if test "${ac_cv_c_o3}" = "no"; then
            CFLAGS_OPTIM="${CFLAGS_OPTIM} -O"
	fi
        CFLAGS_NOOPTIM="${CFLAGS_NOOPTIM} -O"
    fi
fi

dnl Check for -ffast-math
AC_CACHE_CHECK([if \$CC accepts -ffast-math],
    [ac_cv_c_fast_math],
    [CFLAGS="${CFLAGS_save} -ffast-math"
     AC_TRY_COMPILE([],,ac_cv_c_fast_math=yes, ac_cv_c_fast_math=no)])
if test "${ac_cv_c_fast_math}" != "no"; then
    CFLAGS_OPTIM="${CFLAGS_OPTIM} -ffast-math"
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
      CFLAGS_OPTIM="${CFLAGS_OPTIM} -funroll-loops"
  fi
fi

dnl Check for -fomit-frame-pointer
AC_CACHE_CHECK([if \$CC accepts -fomit-frame-pointer],
    [ac_cv_c_omit_frame_pointer],
    [CFLAGS="${CFLAGS_save} -fomit-frame-pointer"
     AC_TRY_COMPILE([],,ac_cv_c_omit_frame_pointer=yes, ac_cv_c_omit_frame_pointer=no)])
if test "${ac_cv_c_omit_frame_pointer}" != "no"; then
    CFLAGS_OPTIM_NODEBUG="${CFLAGS_OPTIM_NODEBUG} -fomit-frame-pointer"
    # this plugin does not compile without -fomit-frame-pointer, damn gcc!
    VLC_ADD_CFLAGS([i420_yuy2_mmx],[-fomit-frame-pointer])
fi

dnl Check for -mdynamic-no-pic
AC_CACHE_CHECK([if \$CC accepts -mdynamic-no-pic],
    [ac_cv_c_dynamic_no_pic],
    [CFLAGS="${CFLAGS_save} -mdynamic-no-pic"
     AC_TRY_COMPILE([],,ac_cv_c_dynamic_no_pic=yes, ac_cv_c_dynamic_no_pic=no)])
if test "${ac_cv_c_dynamic_no_pic}" != "no"; then
    VLC_ADD_CFLAGS([builtin],[-mdynamic-no-pic])
    VLC_ADD_CFLAGS([libvlc],[-mdynamic-no-pic])
fi

dnl Check for Darwin plugin linking flags
AC_CACHE_CHECK([if \$CC accepts -bundle -undefined error -lcc_dynamic],
    [ac_cv_ld_darwin],
    [CFLAGS="${CFLAGS_save} -bundle -undefined error -lcc_dynamic"
     AC_TRY_COMPILE([],,ac_cv_ld_darwin=yes, ac_cv_ld_darwin=no)])
if test "${ac_cv_ld_darwin}" != "no"; then
    VLC_ADD_LDFLAGS([plugin],[-bundle -undefined error])
    AC_CHECK_LIB(cc_dynamic, main,[
      VLC_ADD_LDFLAGS([plugin],[-lcc_dynamic])
    ])
fi

dnl Check for standard plugin linking flags
dnl BeOS' gcc needs -nostart instead of -shared, even if -shared isn't harmful (just a warning)
if test "${SYS}" = "beos"; then
  VLC_ADD_LDFLAGS([plugin mozilla],[-nostart])
else
  AC_CACHE_CHECK([if \$CC accepts -shared],
      [ac_cv_ld_plugins],
      [CFLAGS="${CFLAGS_save} -shared"
       AC_TRY_COMPILE([],, ac_cv_ld_plugins=yes, ac_cv_ld_plugins=no)])
  if test "${ac_cv_ld_plugins}" != "no"; then
    VLC_ADD_LDFLAGS([plugin mozilla],[-shared])
  fi
fi

dnl Check for variadic macros
AC_CACHE_CHECK([for variadic cpp macros],
    [ac_cv_cpp_variadic_macros],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(
         [#include <stdio.h>
          #define a(b,c...) printf(b,##c)],
         [a("foo");a("%s","bar");a("%s%s","baz","quux");],
         ac_cv_cpp_variadic_macros=yes,
         ac_cv_cpp_variadic_macros=no)])
if test "${ac_cv_cpp_variadic_macros}" != "no"; then
    AC_DEFINE(HAVE_VARIADIC_MACROS, 1, Support for variadic macros)
fi

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

dnl Check for __attribute__(())
AC_CACHE_CHECK([for __attribute__((packed))],
  [ac_cv_c_attribute_packed],
  [ac_cv_c_attribute_packed=no
   AC_TRY_COMPILE(, [struct __attribute__((__packed__)) foo { int a; } b;],
                    [ac_cv_c_attribute_packed=yes])])
if test "${ac_cv_c_attribute_packed}" != "no"; then
  AC_DEFINE(HAVE_ATTRIBUTE_PACKED, 1, Support for __attribute__((packed)) for structs)
fi

dnl
dnl  Check the CPU
dnl
case "${target_cpu}" in
  "")
    ARCH=unknown
    ;;
  *)
    ARCH="${target_cpu}"
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
VLC_ADD_PLUGINS([mpgv mpga m4v m4a h264 ps pva avi asf mp4 rawdv nsv real aiff mjpeg demuxdump flac])
VLC_ADD_PLUGINS([cvdsub svcdsub spudec subsdec dvbsub mpeg_audio lpcm a52 dts cinepak flacdec])
VLC_ADD_PLUGINS([deinterlace invert adjust transform distort motionblur rv32])
VLC_ADD_PLUGINS([fixed32tos16 s16tofixed32 u8tofixed32])
VLC_ADD_PLUGINS([trivial_resampler ugly_resampler])
VLC_ADD_PLUGINS([trivial_channel_mixer trivial_mixer])
VLC_ADD_PLUGINS([playlist export sgimb m3u xtag])
VLC_ADD_PLUGINS([i420_rgb rawvideo blend scale image logo])
VLC_ADD_PLUGINS([wav araw subtitle vobsub adpcm a52sys dtssys au ty voc xa nuv])
VLC_ADD_PLUGINS([access_directory access_file access_udp access_tcp])
VLC_ADD_PLUGINS([access_http access_mms access_ftp ipv4])
VLC_ADD_PLUGINS([packetizer_mpegvideo packetizer_h264])
VLC_ADD_PLUGINS([packetizer_mpeg4video packetizer_mpeg4audio])

if test "${SYS}" != "mingwce"; then
dnl  VLC_ADD_PLUGINS([externrun])
  VLC_ADD_PLUGINS([access_fake access_filter_timeshift access_filter_record])
  VLC_ADD_PLUGINS([gestures rc telnet hotkeys netsync showintf time marq sap shout fake])
  VLC_ADD_PLUGINS([rss mosaic wall motiondetect clone crop])
  VLC_ADD_PLUGINS([i420_yuy2 i422_yuy2 i420_ymga])
  VLC_ADD_PLUGINS([aout_file linear_resampler bandlimited_resampler])
  VLC_ADD_PLUGINS([float32_mixer spdif_mixer simple_channel_mixer])
  VLC_ADD_PLUGINS([headphone_channel_mixer normvol equalizer])
  VLC_ADD_PLUGINS([fixed32tofloat32 float32tos16 float32tos8 float32tou16 float32tou8 a52tospdif dtstospdif s16tofloat32 s16tofloat32swab s8tofloat32 u8tofloat32 audio_format])
fi

dnl
dnl  Switch to enable a version of VLC where most modules will be builtin
dnl
AC_ARG_ENABLE(mostly-builtin,
[  --enable-mostly-builtin most modules will be built-in (default disabled)])
if test "${enable_mostly_builtin}" = "yes"; then
  VLC_ADD_BUILTINS([${PLUGINS}])
  PLUGINS=""
fi

dnl
dnl Some plugins aren't useful on some platforms
dnl
if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
    VLC_ADD_PLUGINS([screensaver])
elif test "${SYS}" != "mingwce"; then
    VLC_ADD_PLUGINS([ntservice access_smb dmo])
    VLC_ADD_LDFLAGS([dmo],[-lole32])
else
    VLC_ADD_PLUGINS([win32text])
fi

dnl
dnl  Accelerated modules
dnl
MMX_MODULES="memcpymmx i420_rgb_mmx i422_yuy2_mmx i420_ymga_mmx"
#MMX_MODULES="${MMX_MODULES} idctmmx motionmmx"
MMXEXT_MODULES="memcpymmxext"
#MMXEXT_MODULES="${MMXEXT_MODULES} idctmmxext motionmmxext"
THREEDNOW_MODULES="memcpy3dn"
SSE_MODULES=""
ALTIVEC_MODULES="memcpyaltivec i420_yuy2_altivec"
#ALTIVEC_MODULES="${ALTIVEC_MODULES} idctaltivec motionaltivec"

if test "${enable_gprof}" != "yes"
then
  MMX_MODULES="${MMX_MODULES} i420_yuy2_mmx"
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
if test "${ac_cv_sse_inline}" != "no" -a "${SYS}" != "mingw32" -a "${SYS}" != "solaris"; then
  AC_DEFINE(CAN_COMPILE_SSE, 1, Define if \$CC groks SSE inline assembly.)
  ACCEL_MODULES="${ACCEL_MODULES} ${SSE_MODULES}"
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
    VLC_ADD_CFLAGS([vlc],[${ac_cv_altivec_inline}])
  fi
  ACCEL_MODULES="${ACCEL_MODULES} ${ALTIVEC_MODULES}"
fi

AC_CACHE_CHECK([if \$CC groks AltiVec C extensions],
    [ac_cv_c_altivec],
    [# Darwin test
     CFLAGS="${CFLAGS_save} -faltivec"
     AC_TRY_COMPILE([],
       [vec_ld(0, (unsigned char *)0);],
       [ac_cv_c_altivec="-faltivec"],
       [# Linux/PPC test (no flags)
        CFLAGS="${CFLAGS_save} ${CFLAGS_idctaltivec} -maltivec -mabi=altivec"
        AC_TRY_COMPILE([#ifdef HAVE_ALTIVEC_H
                        #include <altivec.h>
                        #endif],
          [vec_ld(0, (unsigned char *)0);],
          [ac_cv_c_altivec=""
           ac_cv_c_altivec_abi="-maltivec -mabi=altivec"],
          [# Linux/PPC test (old GCC versions)
           CFLAGS="${CFLAGS_save} ${CFLAGS_idctaltivec} -fvec"
           AC_TRY_COMPILE([#ifdef HAVE_ALTIVEC_H
                           #include <altivec.h>
                           #endif],
             [vec_ld(0, (unsigned char *)0);],
             [ac_cv_c_altivec="-fvec"],
             [ac_cv_c_altivec=no])
           ])
        ])
     CFLAGS="${CFLAGS_save}"])
if test "${ac_cv_c_altivec}" != "no"; then
  AC_DEFINE(CAN_COMPILE_C_ALTIVEC, 1, Define if your compiler groks C AltiVec extensions.)
  VLC_ADD_CFLAGS([vlc],[${ac_cv_c_altivec}])
  VLC_ADD_CFLAGS([idctaltivec motionaltivec],[${ac_cv_c_altivec}])
  VLC_ADD_CFLAGS([i420_yuy2_altivec memcpyaltivec deinterlace],[${ac_cv_c_altivec} ${ac_cv_c_altivec_abi}])
  ACCEL_MODULES="${ACCEL_MODULES} ${ALTIVEC_MODULES}"
fi

AC_CACHE_CHECK([if linker needs -framework vecLib],
    [ac_cv_ld_altivec],
    [LDFLAGS="${LDFLAGS_vlc} -framework vecLib"
     AC_TRY_LINK([],,ac_cv_ld_altivec=yes,ac_cv_ld_altivec=no)
     LDFLAGS="${LDFLAGS_save}"
    ])
if test "${ac_cv_ld_altivec}" != "no"; then
  VLC_ADD_LDFLAGS([vlc idctaltivec motionaltivec memcpyaltivec],[-framework vecLib])
fi
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

AC_ARG_WITH(,[])
AC_ARG_WITH(,[Optimization options:])

dnl
dnl  Special arch tuning
dnl
AC_ARG_WITH(tuning,
[  --with-tuning=ARCH      enable special tuning for an architecture
                          (default Pentium 2 on IA-32 and 750 on PPC)])
if test -n "${with_tuning}"; then
    CFLAGS_TUNING="-mtune=${with_tuning}"
else
    if test "${target_cpu}" = "i686" -o "${target_cpu}" = "i586" -o "${target_cpu}" = "i486" -o "${target_cpu}" = "i386"; then
        CFLAGS_TUNING="-mtune=pentium2"
    elif test "${target_cpu}" = "x86_64"; then
        CFLAGS_TUNING="-mtune=athlon64"
    elif test "${target_cpu}" = "powerpc"; then
        CFLAGS_TUNING="-mtune=750";
    fi

    dnl NOTE: this can't be cached cleanly
    AS_IF([test "${CFLAGS_TUNING}"],
       [CFLAGS_save="${CFLAGS}"
        CFLAGS="${CFLAGS} ${CFLAGS_TUNING}"

        AC_MSG_CHECKING([whether $CC accepts ${CFLAGS_TUNING}])
        AC_COMPILE_IFELSE([ ], [tuning="yes"],
                               [CFLAGS_TUNING=""; tuning="no"])

        AC_MSG_RESULT([$tuning])
        CFLAGS="${CFLAGS_save}"
    ])
fi

dnl
dnl  x86 accelerations
dnl
if test "${target_cpu}" = "i686" -o "${target_cpu}" = "i586" -o "${target_cpu}" = "x86" -o "${target_cpu}" = "i386" -o "${target_cpu}" = "x86_64"
then
    ARCH="${ARCH} mmx"
    VLC_ADD_BUILTINS([${ACCEL_MODULES}])
fi

dnl
dnl  Memory usage
dnl
AC_ARG_ENABLE(optimize-memory,
[  --enable-optimize-memory      optimize memory usage over performance])
if test "${enable_optimize_memory}" = "yes"; then
  AC_DEFINE(OPTIMIZE_MEMORY, 1, Define if you want to optimize memory usage over performance)
fi

dnl
dnl  Enable/disable optimizations
dnl
AC_ARG_ENABLE(optimizations,
[  --disable-optimizations disable compiler optimizations (default enabled)])
test "${enable_optimizations}" != "no" && enable_optimizations="yes"

dnl
dnl  AltiVec acceleration
dnl
AC_ARG_ENABLE(altivec,
[  --disable-altivec       disable AltiVec optimizations (default enabled on PPC)],
[ if test "${enable_altivec}" = "yes"; then ARCH="${ARCH} altivec";
    VLC_ADD_BUILTINS([${ACCEL_MODULES}]) fi ],
[ if test "${target_cpu}" = "powerpc"; then ARCH="${ARCH} altivec";
    VLC_ADD_BUILTINS([${ACCEL_MODULES}]) fi ])

dnl
dnl  Debugging mode
dnl
AC_ARG_ENABLE(debug,
[  --enable-debug          debug mode (default disabled)])
test "${enable_debug}" != "yes" && enable_debug="no"

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
  VLC_ADD_PLUGINS([stream_out_duplicate stream_out_gather stream_out_display stream_out_transcode stream_out_bridge stream_out_mosaic_bridge])
#  VLC_ADD_PLUGINS([stream_out_transrate])

  AC_DEFINE(ENABLE_SOUT, 1, Define if you want the stream output support)
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
dnl TLS/SSL
dnl
AC_ARG_ENABLE(gnutls,
  [  --enable-gnutls         gnutls TLS/SSL support (default enabled)])
AS_IF([test "${enable_gnutls}" != "no"], [

  gcrypt_LIBS="-lgpg-error"
  AS_IF([test "${SYS}" = "mingw32"],
    [gcrypt_LIBS="${gcrypt_LIBS} -lws2_32"
     VLC_ADD_LDFLAGS([gnutls],[-lws2_32])]
  )

  gnutls_LIBS="-lgcrypt -lz ${gcrypt_LIBS}"

  AC_CHECK_HEADER(gcrypt.h,
    [AC_CHECK_LIB(gcrypt, gcry_control,
      [AC_CHECK_HEADER(gnutls/gnutls.h,
        [AC_CHECK_LIB(gnutls, gnutls_certificate_verify_peers2,
	  [have_gnutls="yes"],[old_gnutls="yes"], ${gnutls_LIBS})])],,
     ${gcrypt_LIBS})])

  AS_IF([test "${have_gnutls}" = "yes"], [
    VLC_ADD_PLUGINS([gnutls])
    VLC_ADD_LDFLAGS([gnutls], [-lgnutls ${gnutls_LIBS}])
  ], [
    AS_IF([test "${enable_gnutls}" = "yes"], [
      AC_MSG_ERROR([gnutls not present or too old (version 1.0.17 required)])]
    )]
  )]
)


dnl
dnl  Input plugins
dnl

AC_ARG_WITH(,[Input plugins:])

dnl  live.com input
dnl
AC_ARG_ENABLE(livedotcom,
[  --enable-livedotcom     live.com input plugin (default disabled)])
if test "${enable_livedotcom}" = "yes"; then
  AC_ARG_WITH(livedotcom-tree,
    [    --with-livedotcom-tree=PATH live.com tree for static linking])

  dnl
  dnl test for --with-livedotcom-tree
  dnl
  if test -z "${with_livedotcom_tree}" -a "${CXX}" != ""; then
    AC_LANG_PUSH(C++)
    CPPFLAGS_save="${CPPFLAGS}"
    CPPFLAGS_livedotcom="-I/usr/include/liveMedia -I/usr/include/groupsock -I/usr/include/BasicUsageEnvironment -I/usr/include/UsageEnvironment"
    CPPFLAGS="${CPPFLAGS} ${CPPFLAGS_livedotcom}"
    AC_CHECK_HEADERS(liveMedia.hh, [
      VLC_ADD_CXXFLAGS([livedotcom], [${CPPFLAGS_livedotcom}])
      AC_CHECK_LIB(liveMedia_pic, main, [
        # We have -lliveMedia_pic, build plugins
        VLC_ADD_PLUGINS([livedotcom])
        VLC_ADD_LDFLAGS([livedotcom], [-lliveMedia_pic -lgroupsock_pic -lBasicUsageEnvironment_pic -lUsageEnvironment_pic])
      ], [
        AC_CHECK_LIB(liveMedia, main, [
          # We only have -lliveMedia, do builtins
          VLC_ADD_BUILTINS([livedotcom])
          VLC_ADD_LDFLAGS([livedotcom], [-lliveMedia -lgroupsock -lBasicUsageEnvironment -lUsageEnvironment])
        ])
      ])
      if test "${SYS}" = "mingw32"; then
        # add ws2_32 for closesocket, select, recv
        VLC_ADD_LDFLAGS([livedotcom],[-lws2_32])
      fi
    ])
    CPPFLAGS="${CPPFLAGS_save}"
    AC_LANG_POP(C++)
  else
    AC_MSG_CHECKING(for liveMedia/libliveMedia.a in ${with_livedotcom_tree})
    real_livedotcom_tree="`cd ${with_livedotcom_tree} 2>/dev/null && pwd`"
    if test -z "${real_livedotcom_tree}"; then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_livedotcom_tree}])
    fi
    if test -f "${real_livedotcom_tree}/liveMedia/libliveMedia.a"; then
      AC_MSG_RESULT(${real_livedotcom_tree}/liveMedia/libliveMedia.a)

      VLC_ADD_BUILTINS([livedotcom])

      if test "${SYS}" = "mingw32"; then
        # add ws2_32 for closesocket, select, recv
        VLC_ADD_LDFLAGS([livedotcom],[-lws2_32])
      fi

      VLC_ADD_LDFLAGS([livedotcom],[-L${real_livedotcom_tree}/liveMedia -lliveMedia])
      VLC_ADD_LDFLAGS([livedotcom],[-L${real_livedotcom_tree}/BasicUsageEnvironment -lBasicUsageEnvironment])
      VLC_ADD_LDFLAGS([livedotcom],[-L${real_livedotcom_tree}/groupsock -lgroupsock])
      VLC_ADD_LDFLAGS([livedotcom],[-L${real_livedotcom_tree}/UsageEnvironment -lUsageEnvironment])

      VLC_ADD_CXXFLAGS([livedotcom],[-I${real_livedotcom_tree}/BasicUsageEnvironment/include])
      VLC_ADD_CXXFLAGS([livedotcom],[-I${real_livedotcom_tree}/groupsock/include])
      VLC_ADD_CXXFLAGS([livedotcom],[-I${real_livedotcom_tree}/liveMedia/include])
      VLC_ADD_CXXFLAGS([livedotcom],[-I${real_livedotcom_tree}/UsageEnvironment/include ])
    else
      dnl  The given live.com wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_livedotcom_tree}/liveMedia/libliveMedia.a, make sure you compiled live.com in ${with_livedotcom_tree}])
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
      VLC_ADD_LDFLAGS([dvdread],[-ldvdcss])
  fi

  if test -z "${with_dvdread}"
  then
    if test -z "${with_dvdread_tree}"
    then
      AC_CHECK_HEADERS(dvdread/dvd_reader.h,
        [ VLC_ADD_PLUGINS([dvdread])
          VLC_ADD_LDFLAGS([dvdread],[-ldvdread ${LDFLAGS_dvdcss}])
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
        VLC_ADD_LDFLAGS([dvdread],[-L${real_dvdread_tree}/dvdread/.libs -ldvdread ${LDFLAGS_dvdcss}])
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
      VLC_ADD_LDFLAGS([dvdread],[-L${with_dvdread}/lib -ldvdread ${LDFLAGS_dvdcss}])
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
      VLC_ADD_LDFLAGS([dvdnav],[-ldvdcss])
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
    VLC_ADD_LDFLAGS([dvdnav],[`${DVDNAV_CONFIG} --libs`])
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
      AC_CHECK_HEADERS(dshow.h,
      [ VLC_ADD_PLUGINS([dshow])
        VLC_ADD_CXXFLAGS([dshow],[])
        VLC_ADD_LDFLAGS([dshow],[-lole32 -loleaut32 -luuid]) ])
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
      VLC_ADD_LDFLAGS([access_smb],[-lsmbclient]) ],
    [ if test -n "${enable_smb}"; then
        AC_MSG_ERROR([cannot find libsmbclient headers])
     fi ])
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
            VLC_ADD_BUILTINS([mux_ts])
          fi
          VLC_ADD_LDFLAGS([mux_ts ts dvb],[-ldvbpsi]) ],
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
        VLC_ADD_LDFLAGS([mux_ts ts dvb],[-L${real_dvbpsi_tree}/src/.libs -ldvbpsi])
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
      VLC_ADD_LDFLAGS([mux_ts ts dvb],[${LDFLAGS_test} -ldvbpsi])

    ],[
      if test -n "${enable_dvbpsi}"
      then
        AC_MSG_ERROR([Could not find libdvbpsi on your system: you may get it from www.videolan.org, you'll need at least version 0.1.1])
      fi
    ])
    CPPFLAGS="${CPPFLAGS_save}"
  ;;
  esac
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
dnl  special access module for Hauppauge PVR cards
dnl
AC_ARG_ENABLE(pvr,
  [  --enable-pvr            PVR cards access module (default disabled)])
if test "${enable_pvr}" = "yes"
then
  VLC_ADD_PLUGINS([pvr])
fi

dnl Need to test libcdio and libvcdinfo for a number of things. Do it now.
AC_ARG_ENABLE(libcdio,
  [  --enable-libcdio        CD input and control library support (default enabled)])
 
have_libcdio=no
have_libvcdinfo=no
if test "${enable_libcdio}" != "no"
then
  PKG_CHECK_MODULES(LIBCDIO, libcdio >= 0.72,
     have_libcdio=yes
     AC_DEFINE(HAVE_LIBCDIO, [], 
     [Define if you have libcdio 0.72 or greater installed]),
     [AC_MSG_WARN(CD Reading and information library not found)])

  PKG_CHECK_MODULES(VCDINFO, libvcdinfo >= 0.7.21,
     [have_libvcdinfo=yes
     AC_DEFINE(HAVE_VCDINFO, [], 
     [Define if you have libvcdinfo 0.7.21 or greater installed])],
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
    AC_DEFINE(HAVE_CDDAX, [], [Define for the audio CD plugin using libcdio])
    VLC_ADD_LDFLAGS([cddax],[$LIBCDIO_LIBS])
    VLC_ADD_CFLAGS([cddax],[$LIBCDIO_CFLAGS])
    VLC_ADD_PLUGINS([cddax])
    PKG_CHECK_MODULES(LIBCDIO_PARANOIA, libcdio_paranoia >= 0.72, [
    VLC_ADD_LDFLAGS([cddax],[$LIBCDIO_CDDA_LIBS $LIBCDIO_CDDA_LIBS $LIBCDIO_PARANOIA_LIBS])],
    AC_MSG_WARN([CD Paranoia support disabled because no libcdio >= 0.72 found]))
  else 
    AC_MSG_WARN([cddax plugin disabled because ok libcdio library not found or disabled])
    HAVE_CDDAX=no
  fi

  if test "$enable_libcddb" != "no"; then
    PKG_CHECK_MODULES(LIBCDDB, libcddb >= 0.9.5, [
      HAVE_LIBCDDB=yes 
      AC_DEFINE(HAVE_LIBCDDB, [], [Define this if you have libcddb installed])
      VLC_ADD_LDFLAGS([cddax],[$LIBCDDB_LIBS])
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
    VLC_ADD_LDFLAGS([vcdx],[$VCDINFO_LIBS])
    VLC_ADD_CFLAGS([vcdx],[$VCDINFO_CFLAGS])
  else 
    AC_MSG_WARN([vcdx plugin disabled because ok libvcdinfo library not found or disabled])
    HAVE_VCDX=no
  fi

  PKG_CHECK_MODULES(LIBCDIO, libiso9660 >= 0.72,
   [VLC_ADD_LDFLAGS([vcdx],[$LIBISO9660_LIBS])
    VLC_ADD_CFLAGS([vcdx],[$LIBISO9660_CFLAGS])],
    [AC_MSG_WARN([vcdx plugin disabled because ok libiso9660 library not found])
    HAVE_VCDX=no])

  if test "$have_libvcdinfo" = "yes"
  then
    AC_DEFINE(HAVE_VCDX, [], 
    [Define for the VCD plugin using libcdio/libvcdinfo])
    VLC_ADD_LDFLAGS([vcdx],[$VCDINFO_LIBS])
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
    VLC_ADD_LDFLAGS([vcd vcdx cdda cddax],[-framework IOKit -framework CoreFoundation])
    VLC_ADD_LDFLAGS([vcdx cddax],[-liconv])
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
  ],[])
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
      VLC_ADD_LDFLAGS([screen],[-framework ApplicationServices])
    ])
  elif test "${SYS}" = "mingw32"; then
    VLC_ADD_PLUGINS([screen])
    VLC_ADD_LDFLAGS([screen],[-lgdi32])
  elif test "${SYS}" = "mingwce"; then
    CPPFLAGS="${CPPFLAGS_save}"
  elif test "${SYS}" = "beos"; then
    VLC_ADD_PLUGINS([screen])
    VLC_ADD_CXXFLAGS([screen],[])
    VLC_ADD_LDFLAGS([screen],[-lbe])
  else
    CPPFLAGS="${CPPFLAGS_save} -I${x_includes}"
    AC_CHECK_HEADERS(X11/Xlib.h, [
      VLC_ADD_PLUGINS([screen])
      VLC_ADD_LDFLAGS([screen],[-L${x_libraries} -lX11 -lXext])
      VLC_ADD_CPPFLAGS([screen],[-I${x_includes}])
    ])
    CPPFLAGS="${CPPFLAGS_save}"
  fi
fi

dnl
dnl  ipv6 plugin - not for QNX yet
dnl
have_ipv6=no
AC_CHECK_FUNCS(inet_pton,[have_ipv6=yes],[
  AC_CHECK_LIB(resolv,inet_pton,
    [have_ipv6=yes
     VLC_ADD_LDFLAGS([ipv6],[-lresolv])])
])

AS_IF([test "${have_ipv6}" == "yes"], [
  AC_DEFINE(HAVE_INET_PTON, 1, [Define to 1 if you have inet_pton().])])

if test "${SYS}" != "nto" &&
   test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"
then
  AC_MSG_CHECKING(for sockaddr_in6 in netinet/in.h)
  AC_EGREP_HEADER(sockaddr_in6,netinet/in.h,
    [AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no); have_ipv6=no])
  AS_IF([test "${have_ipv6}" != "no"], [
    VLC_ADD_PLUGINS([ipv6])])
fi
if test "${SYS}" = "mingw32"
then
  AC_MSG_CHECKING(for getaddrinfo in ws2tcpip.h)
  AC_EGREP_HEADER(addrinfo,ws2tcpip.h,[AC_MSG_RESULT(yes)
    VLC_ADD_PLUGINS([ipv6])],[AC_MSG_RESULT(no)])
fi

dnl
dnl  ogg demux plugin
dnl
AC_ARG_ENABLE(ogg,
  [  --enable-ogg            Ogg demux support (default enabled)])
if test "${enable_ogg}" != "no"
then
  AC_CHECK_HEADERS(ogg/ogg.h, [
    AC_CHECK_LIB( ogg, oggpack_read, [
      VLC_ADD_PLUGINS([ogg])
      if test "${enable_sout}" != "no"; then
        VLC_ADD_PLUGINS([mux_ogg])
      fi
      VLC_ADD_LDFLAGS([ogg mux_ogg],[-logg])])
   ],[])
fi

dnl
dnl  matroska demux plugin
dnl
AC_ARG_ENABLE(mkv,
  [  --enable-mkv            Matroska demux support (default enabled)])
if test "${enable_mkv}" != "no" -a "${CXX}" != ""; then
  AC_LANG_PUSH(C++)
  AC_CHECK_HEADERS(ebml/EbmlVersion.h, [
    AC_MSG_CHECKING(for libebml version >= 0.7.3)
    AC_EGREP_CPP(yes,
      [#include <ebml/EbmlVersion.h>
       #ifdef LIBEBML_VERSION
       #if LIBEBML_VERSION >= 0x000703
       yes
       #endif
       #endif],
      [AC_MSG_RESULT([yes])
        AC_CHECK_HEADERS(matroska/KaxVersion.h, [
          AC_MSG_CHECKING(for libmatroska version >= 0.7.5)
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
                # We have ebml_pic, that's good, we can build an mkv.so plugin !
                VLC_ADD_PLUGINS([mkv])
                VLC_ADD_LDFLAGS([mkv],[-lmatroska_pic -lebml_pic])
              ], [
                AC_CHECK_LIB(ebml, main, [
                  # We only have libebml, make mkv.a a builtin
                  VLC_ADD_BUILTINS([mkv])
                  VLC_ADD_LDFLAGS([mkv],[-lmatroska -lebml])
                ])
              ])
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
  AC_CHECK_HEADERS(libmodplug/modplug.h, [
    VLC_ADD_PLUGINS([mod])
    VLC_ADD_CXXFLAGS([mod],[])
    VLC_ADD_LDFLAGS([mod],[-lmodplug])])
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
    VLC_ADD_LDFLAGS([mpgatofixed32],[-L${with_mad}/lib])
  fi

  AC_ARG_WITH(mad-tree,
    [    --with-mad-tree=PATH  mad tree for static linking],[],[])
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
      VLC_ADD_LDFLAGS([mpgatofixed32],[-L${real_mad_tree}/.libs])
      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_mpgatofixed32}"
      AC_CHECK_LIB(mad, mad_bit_init, [
        VLC_ADD_BUILTINS([mpgatofixed32])
        VLC_ADD_LDFLAGS([mpgatofixed32],[-lmad])
        ],[ AC_MSG_ERROR([the specified tree hasn't been compiled ])
      ],[])
      LDFLAGS="${LDFLAGS_save}"
    else
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([the specified tree doesn't have mad.h])
    fi
  else
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_mpgatofixed32}"
    LDFLAGS="${LDFLAGS_save} ${LDFLAGS_mpgatofixed32}"
    AC_CHECK_HEADERS(mad.h, ,
      [ AC_MSG_ERROR([Could not find libmad on your system: you may get it from http://www.underbit.com/products/mad/. Alternatively you can use --disable-mad to disable the mad plugin.]) ])
    AC_CHECK_LIB(mad, mad_bit_init, [
      VLC_ADD_PLUGINS([mpgatofixed32])
      VLC_ADD_LDFLAGS([mpgatofixed32],[-lmad])],
      [ AC_MSG_ERROR([Cannot find libmad library...]) ])
    CPPFLAGS="${CPPFLAGS_save}"
    LDFLAGS="${LDFLAGS_save}"
  fi
fi

dnl
dnl   libid3tag support (FIXME!!! doesn't work with new input)
dnl
AC_CHECK_HEADERS(id3tag.h, [
  AC_CHECK_HEADERS(zlib.h, [
    VLC_ADD_LDFLAGS([id3tag],[-lid3tag -lz])
    VLC_ADD_PLUGINS([id3tag])]) ])

dnl
dnl  ffmpeg decoder/demuxer plugin
dnl
AC_ARG_ENABLE(ffmpeg,
[  --enable-ffmpeg         ffmpeg codec (default enabled)])
if test "${enable_ffmpeg}" != "no"
then
dnl Look for a ffmpeg-config (we are on debian )
 
   FFMPEG_PATH="${PATH}"
   AC_ARG_WITH(ffmpeg-config-path,
     [    --with-ffmpeg-config-path=PATH ffmpeg-config path (default search in \$PATH)],
     [ if test "${with_ffmpeg_config_path}" != "no"
       then
         FFMPEG_PATH="${with_ffmpeg_config_path}"
       fi ])
   AC_PATH_PROG(FFMPEG_CONFIG, ffmpeg-config, no, ${FFMPEG_PATH})
   if test "${FFMPEG_CONFIG}" != "no"
   then
     AC_CHECK_HEADERS(ffmpeg/avcodec.h)
     AC_CHECK_HEADERS(postproc/postprocess.h)
     VLC_ADD_PLUGINS([ffmpeg])
     if test "${enable_sout}" != "no"; then
         VLC_ADD_PLUGINS([stream_out_switcher])
     fi
     VLC_ADD_CFLAGS([ffmpeg stream_out_switcher],[`${FFMPEG_CONFIG} --cflags`])
     VLC_ADD_LDFLAGS([ffmpeg stream_out_switcher],[`${FFMPEG_CONFIG} --plugin-libs avcodec avformat postproc`])
  
  else
    AC_ARG_WITH(ffmpeg-mp3lame,
      [    --with-ffmpeg-mp3lame specify if ffmpeg has been compiled with mp3lame support],
      [
        VLC_ADD_LDFLAGS([ffmpeg],[-lmp3lame]) ])

    AC_ARG_WITH(ffmpeg-faac,
      [    --with-ffmpeg-faac    specify if ffmpeg has been compiled with faac support],
      [
        VLC_ADD_LDFLAGS([ffmpeg],[-lfaac]) ])

    AC_ARG_WITH(ffmpeg-dts,
      [    --with-ffmpeg-dts     specify if ffmpeg has been compiled with dts support],
      [
        VLC_ADD_LDFLAGS([ffmpeg],[-ldts]) ])

    AC_ARG_WITH(ffmpeg-zlib,
      [    --with-ffmpeg-zlib    specify if ffmpeg has been compiled with zlib support],
      [
        VLC_ADD_LDFLAGS([ffmpeg],[-lz]) ])

    AC_ARG_WITH(ffmpeg-tree,
      [    --with-ffmpeg-tree=PATH ffmpeg tree for static linking])

    dnl
    dnl test for !(--with-ffmpeg-tree)
    dnl
    if test "${with_ffmpeg_tree}" = "no" -o -z "${with_ffmpeg_tree}"; then
      CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_ffmpeg}"
      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_ffmpeg}"
      AC_CHECK_HEADERS(ffmpeg/avcodec.h, [], [AC_MSG_ERROR([Missing header file ffmpeg/avcodec.h.])] )
      AC_CHECK_HEADERS(postproc/postprocess.h, [], [AC_MSG_ERROR([Missing header file postproc/postprocess.h.])] )

      AC_CHECK_LIB(avutil, av_add_i, [
        VLC_ADD_LDFLAGS([ffmpeg],[-lavutil])
        LDAVUTIL="-lavutil"])

      AC_CHECK_LIB(avcodec, avcodec_init, [
        VLC_ADD_BUILTINS([ffmpeg])
        VLC_ADD_LDFLAGS([ffmpeg],[-lavcodec])
        if test "${enable_sout}" != "no"; then
            VLC_ADD_BUILTINS([stream_out_switcher])
        fi],
         [ AC_MSG_ERROR([Could not find ffmpeg on your system: you may get it from http://ffmpeg.sf.net/ (cvs version is recommended). Alternatively you can use --disable-ffmpeg to disable the ffmpeg plugins.]) ], [$LDAVUTIL])
      AC_CHECK_LIB(avformat, av_open_input_stream, [
        AC_DEFINE(HAVE_LIBAVFORMAT, 1,
        [Define if you have ffmpeg's libavformat.])
        VLC_ADD_LDFLAGS([ffmpeg],[-lavformat -lz]) ], [], [-lavcodec -lz])
      LDFLAGS="${LDFLAGS_save}"
      CPPFLAGS="${CPPFLAGS_save}"
    fi

    dnl
    dnl test for --with-ffmpeg-tree
    dnl
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
        dnl  The given libavcodec wasn't built with --enable-pp
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([${real_ffmpeg_tree}/libavcodec/libavcodec.a was not compiled with postprocessing support, make sure you configured ffmpeg with --enable-pp])
      fi
      dnl  Use a custom libffmpeg
      AC_MSG_RESULT(${real_ffmpeg_tree}/libavcodec/libavcodec.a)

      if fgrep -s "CONFIG_ZLIB=yes" "${real_ffmpeg_tree}/config.mak"; then
        if test "${with_ffmpeg_zlib}" != "yes"; then
          VLC_ADD_LDFLAGS([ffmpeg],[-lz])
        fi
      fi
      if fgrep -s "CONFIG_MP3LAME=yes" "${real_ffmpeg_tree}/config.mak"; then
        if test "${with_ffmpeg_mp3lame}" != "yes"; then
          VLC_ADD_LDFLAGS([ffmpeg],[-lmp3lame])
        fi
      fi
      if fgrep -s "CONFIG_FAAC=yes" "${real_ffmpeg_tree}/config.mak"; then
        if test "${with_ffmpeg_faac}" != "yes"; then
          VLC_ADD_LDFLAGS([ffmpeg],[-lfaac])
        fi
      fi
      if fgrep -s "CONFIG_DTS=yes" "${real_ffmpeg_tree}/config.mak"; then
        if test "${with_ffmpeg_dts}" != "yes"; then
          VLC_ADD_LDFLAGS([ffmpeg],[-ldts])
        fi
      fi
      if fgrep -s "CONFIG_VORBIS=yes" "${real_ffmpeg_tree}/config.mak"; then
        VLC_ADD_LDFLAGS([ffmpeg],[-lvorbis -lvorbisenc])
      fi
      if fgrep -s "CONFIG_FAAD=yes" "${real_ffmpeg_tree}/config.mak"; then
        VLC_ADD_LDFLAGS([ffmpeg],[-lfaad])
      fi
      if fgrep -s "CONFIG_XVID=yes" "${real_ffmpeg_tree}/config.mak"; then
        VLC_ADD_LDFLAGS([ffmpeg],[-lxvidcore])
      fi

      VLC_ADD_BUILTINS([ffmpeg])
      if test "${enable_sout}" != "no"; then
        VLC_ADD_BUILTINS([stream_out_switcher])
      fi

      if test -f "${real_ffmpeg_tree}/libavutil/libavutil.a"; then
        VLC_ADD_LDFLAGS([ffmpeg],[-L${real_ffmpeg_tree}/libavutil ${real_ffmpeg_tree}/libavutil/libavutil.a])
        VLC_ADD_CPPFLAGS([ffmpeg stream_out_switcher],[-I${real_ffmpeg_tree}/libavutil])
      fi

      VLC_ADD_LDFLAGS([ffmpeg],[-L${real_ffmpeg_tree}/libavcodec ${real_ffmpeg_tree}/libavcodec/libavcodec.a])
      VLC_ADD_CPPFLAGS([ffmpeg stream_out_switcher],[-I${real_ffmpeg_tree}/libavcodec])

      if test -f "${real_ffmpeg_tree}/libavformat/libavformat.a"; then
        AC_DEFINE(HAVE_LIBAVFORMAT, 1, [Define if you have ffmpeg's libavformat.])
        VLC_ADD_LDFLAGS([ffmpeg],[-L${real_ffmpeg_tree}/libavformat ${real_ffmpeg_tree}/libavformat/libavformat.a])
        VLC_ADD_CPPFLAGS([ffmpeg stream_out_switcher],[-I${real_ffmpeg_tree}/libavformat])
      fi
    fi
  fi
fi

dnl
dnl  ffmpeg decoder/demuxer plugin
dnl
AC_ARG_ENABLE(ffmpegaltivec,
[  --enable-ffmpegaltivec  ffmpegaltivec codec (DO NOT USE)])
if test "${enable_ffmpegaltivec}" == "yes"
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
    VLC_ADD_LDFLAGS([ffmpegaltivec],[-L${real_ffmpeg_tree}/libavcodec -lavcodecaltivec])
    VLC_ADD_CPPFLAGS([ffmpeg],[-DNO_ALTIVEC_IN_FFMPEG])
    VLC_ADD_CPPFLAGS([ffmpegaltivec],[-I${real_ffmpeg_tree}/libavcodec -I${real_ffmpeg_tree}/libavformat])

    if test -f "${real_ffmpeg_tree}/libavformat/libavformat.a"; then
      AC_DEFINE(HAVE_LIBAVFORMAT, 1, [Define if you have ffmpeg's libavformat.])
      VLC_ADD_LDFLAGS([ffmpegaltivec],[-L${real_ffmpeg_tree}/libavformat -lavformataltivec -lz])
      VLC_ADD_CPPFLAGS([ffmpegaltivec],[-I${real_ffmpeg_tree}/libavformat])
    fi
    VLC_ADD_LDFLAGS([stream_out_switcher],[-L${real_ffmpeg_tree}/libavcodec])
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
      VLC_ADD_LDFLAGS([faad],[${real_faad_tree}/libfaad/.libs/libfaad.a])
      VLC_ADD_CPPFLAGS([faad],[-I${real_faad_tree}/include])
    else
      dnl  The given libfaad wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_faad_tree}/libfaad/.libs/libfaad.a, make sure you compiled libfaad in ${with_faad_tree}])
    fi
  else
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_faad}"
    LDFLAGS="${LDFLAGS_save} ${LDFLAGS_faad}"
    AC_CHECK_HEADERS(faad.h, ,
      [ AC_MSG_ERROR([Cannot find development headers for libfaad...]) ])
    AC_CHECK_LIB(faad, faacDecOpen, [
      VLC_ADD_PLUGINS([faad])
      VLC_ADD_LDFLAGS([faad],[-lfaad]) ],
      AC_CHECK_LIB(faad, NeAACDecOpen, [
        VLC_ADD_PLUGINS([faad])
        VLC_ADD_LDFLAGS([faad],[-lfaad]) ],
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
    if test -f "${real_twolame_tree}/src/libtwolame/.libs/libtwolame.a"
    then
      dnl  Use a custom twolame
      AC_MSG_RESULT(${real_twolame_tree}/src/libtwolame/.libs/libtwolame.a)
      VLC_ADD_BUILTINS([twolame])
      VLC_ADD_LDFLAGS([twolame],[${real_twolame_tree}/src/libtwolame/.libs/libtwolame.a])
      VLC_ADD_CPPFLAGS([twolame],[-I${real_twolame_tree}/src/libtwolame])
    else
      dnl  The given libtwolame wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_twolame_tree}/src/libtwolame/.libs/libtwolame.a, make sure you compiled libtwolame in ${with_twolame_tree}])
    fi
  else
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_twolame}"
    LDFLAGS="${LDFLAGS_save} ${LDFLAGS_twolame}"
    AC_CHECK_HEADERS(twolame.h, ,
      [ AC_MSG_ERROR([Cannot find development header for libtwolame...]) ])
    AC_CHECK_LIB(twolame, twolame_init, [
      VLC_ADD_PLUGINS([twolame])
      VLC_ADD_LDFLAGS([twolame],[-ltwolame]) ],
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
      VLC_ADD_LDFLAGS([quicktime],[-framework QuickTime -framework Carbon])
    ], [ AC_MSG_ERROR([cannot find QuickTime headers]) ])
  fi
fi

dnl
dnl MP4 module
dnl
AC_CHECK_HEADERS(zlib.h, [
  VLC_ADD_LDFLAGS([mp4 skins2 sap mkv],[-lz])
] )

AC_CHECK_HEADERS(sysfs/libsysfs.h, [
  VLC_ADD_LDFLAGS([mp4 mkv],[-lsysfs])
] )

dnl
dnl skins2 module
dnl
AC_CHECK_HEADERS(libtar.h, [
  VLC_ADD_LDFLAGS([skins2],[-ltar])
] )



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
      VLC_ADD_LDFLAGS([a52tofloat32],[-L${real_a52_tree}/liba52/.libs])
      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_a52tofloat32}"
      AC_CHECK_LIB(a52, a52_free, [
        VLC_ADD_BUILTINS([a52tofloat32])
        VLC_ADD_CPPFLAGS([a52tofloat32],[-DUSE_A52DEC_TREE])
        VLC_ADD_LDFLAGS([a52tofloat32],[-la52])
        ],[
        if test -f ${real_a52_tree}/liba52/.libs/liba52.a
        then
          AC_MSG_ERROR([make sure you have at least a52dec-0.7.3])
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
    LDFLAGS="${LDFLAGS_save} ${LDFLAGS_test} ${LDFLAGS_a52tofloat32}"
    AC_CHECK_HEADERS(a52dec/a52.h, [
      AC_CHECK_LIB(a52, a52_free, [
        VLC_ADD_PLUGINS([a52tofloat32])
        VLC_ADD_LDFLAGS([a52tofloat32],[${LDFLAGS_test} -la52])
        VLC_ADD_CPPFLAGS([a52tofloat32],[${CPPFLAGS_test}])
      ],[
        AC_MSG_ERROR([Could not find liba52 on your system: you may get it from http://liba52.sf.net/. Alternatively you can use --disable-a52 to disable the a52 plugin.])
      ])
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
AC_ARG_ENABLE(dts,
  [  --enable-dts            DTS Coherent Acoustics support with libdts (default enabled)])
if test "${enable_dts}" != "no"; then
  AC_ARG_WITH(dts-tree,
    [    --with-dts-tree=PATH  libdts tree for static linking ],[],[])
  if test "${with_dts_tree}" != "no" -a -n "${with_dts_tree}"
  then
    real_dts_tree="`cd ${with_dts_tree} 2>/dev/null && pwd`"
    if test -z "${real_dts_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([${with_dts_tree} directory doesn't exist])
    fi
    dnl  Use a custom libdts
    AC_MSG_CHECKING(for dts.h in ${real_dts_tree}/include)
    if test -f ${real_dts_tree}/include/dts.h
    then
      AC_MSG_RESULT(yes)
      VLC_ADD_CPPFLAGS([dtstofloat32],[-I${real_dts_tree}/include])
      VLC_ADD_LDFLAGS([dtstofloat32],[-L${real_dts_tree}/libdts])
      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_dtstofloat32}"
      AC_CHECK_LIB(dts_pic, dts_free, [
        VLC_ADD_PLUGINS([dtstofloat32])
        VLC_ADD_LDFLAGS([dtstofloat32],[-ldts_pic])
        ],[
        AC_CHECK_LIB(dts, dts_free, [
          VLC_ADD_BUILTINS([dtstofloat32])
          VLC_ADD_LDFLAGS([dtstofloat32],[-ldts])
          ],[
          if test -f ${real_dts_tree}/libdts/libdts.a
          then
            AC_MSG_ERROR([make sure you have at least libdts-0.0.2])
          else
            AC_MSG_ERROR([the specified tree hasn't been compiled])
          fi
        ])
      ])
      LDFLAGS="${LDFLAGS_save}"
    else
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([the specified tree doesn't have dts.h])
    fi
  else
    LDFLAGS="${LDFLAGS_save} ${LDFLAGS_dtstofloat32}"
    AC_CHECK_HEADERS(dts.h, [
      AC_CHECK_LIB(dts_pic, dts_free, [
        VLC_ADD_PLUGINS([dtstofloat32])
        VLC_ADD_LDFLAGS([dtstofloat32],[-ldts_pic])
      ],[
        AC_CHECK_LIB(dts, dts_free, [
          VLC_ADD_BUILTINS([dtstofloat32])
          VLC_ADD_LDFLAGS([dtstofloat32],[-ldts])
        ],[
          if test "${enable_dts}" = "yes"; then
            AC_MSG_ERROR([Could not find libdts on your system: you may get it from http://www.videolan.org/dtsdec.html])
          fi
        ])
      ])
    ])
    LDFLAGS="${LDFLAGS_save}"
  fi
fi

dnl
dnl  Flac plugin
dnl
AC_ARG_ENABLE(flac,
  [  --enable-flac           flac decoder support (default disabled)])
if test "${enable_flac}" = "yes"
then
  AC_CHECK_HEADERS(FLAC/stream_decoder.h, [
    VLC_ADD_LDFLAGS([flacdec],[-lFLAC])
   ],[])
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
      VLC_ADD_LDFLAGS([libmpeg2],[-L${real_libmpeg2_tree}/libmpeg2/.libs -lmpeg2])
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
          VLC_ADD_LDFLAGS([libmpeg2],[-lmpeg2])],
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
  AC_CHECK_HEADERS(vorbis/codec.h, [
    VLC_ADD_PLUGINS([vorbis])
    VLC_ADD_LDFLAGS([vorbis],[-lvorbis -logg]) ],[])

  AC_CHECK_HEADERS(vorbis/vorbisenc.h, [
    VLC_ADD_LDFLAGS([vorbis],[-lvorbisenc]) ],[])
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
    VLC_ADD_LDFLAGS([tremor],[-lvorbisidec -logg])
   ],[])
fi

dnl
dnl  Speex plugin
dnl
AC_ARG_ENABLE(speex,
  [  --enable-speex          Speex decoder support (default enabled)])
if test "${enable_speex}" != "no"
then
  AC_CHECK_HEADERS(speex/speex.h, [
    LDFLAGS="${LDFLAGS_save} ${LDFLAGS_speex}"
    AC_CHECK_LIB(speex, speex_decode_int, [
      VLC_ADD_PLUGINS([speex])
      VLC_ADD_LDFLAGS([speex],[-lspeex]) ],
      [ AC_MSG_RESULT([no])
        AC_MSG_WARN([Your libspeex is too old, please get the development
                     version.]) ],[])
    LDFLAGS="${LDFLAGS_save}"
    ],[])
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
      VLC_ADD_LDFLAGS([tarkin],[${real_tarkin_tree}/mem.o ${real_tarkin_tree}/pnm.o ${real_tarkin_tree}/wavelet.o ${real_tarkin_tree}/wavelet_xform.o ${real_tarkin_tree}/wavelet_coeff.o ${real_tarkin_tree}/yuv.o ${real_tarkin_tree}/tarkin.o ${real_tarkin_tree}/info.o -logg])
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
      if test "${SYS}" = "mingw32"; then
        VLC_ADD_PLUGINS([theora])
      else
        VLC_ADD_BUILTINS([theora])
      fi
      theora_libs="-ltheora -logg"
      VLC_ADD_LDFLAGS([theora],[${theora_libs}]) ],[
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
  AC_CHECK_HEADERS(libdirac_decoder/dirac_parser.h, [
    AC_CHECK_LIB(dirac_decoder, dirac_decoder_init, [
      VLC_ADD_PLUGINS([dirac])
      VLC_ADD_LDFLAGS([dirac],[-ldirac_decoder -ldirac_encoder -lstdc++]) ],[
      AC_MSG_ERROR([libdirac doesn't appear to be installed on you system.])],
      [-lstdc++])
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
    VLC_ADD_LDFLAGS([png],[-lpng -lz])
    VLC_ADD_PLUGINS([png])
    VLC_ADD_PLUGINS([osdmenu])
    AC_DEFINE(HAVE_LIBPNG, [], [Define if you have the PNG library: libpng])],
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
      VLC_ADD_LDFLAGS([x264],[-L${real_x264_tree}])
      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_x264}"
      AC_CHECK_LIB(x264, x264_encoder_open, [
        VLC_ADD_BUILTINS([x264])
        VLC_ADD_LDFLAGS([x264],[-lx264])
      ],[
        AC_MSG_ERROR([the specified tree hasn't been compiled])
      ])
      LDFLAGS="${LDFLAGS_save}"
    else
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([the specified tree doesn't have x264.h])
    fi
  else
    LDFLAGS="${LDFLAGS_save} ${LDFLAGS_x264}"
    AC_CHECK_HEADERS(x264.h, [
      AC_CHECK_LIB(x264, x264_encoder_open, [
        VLC_ADD_PLUGINS([x264])
        VLC_ADD_LDFLAGS([x264],[-lx264])
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
  CPPFLAGS="${CPPFLAGS_save} -I${x_includes}"
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
  CPPFLAGS="${CPPFLAGS_save} -I${x_includes}"
  AC_CHECK_HEADERS(X11/Xlib.h, [
    VLC_ADD_PLUGINS([x11])
    VLC_ADD_LDFLAGS([x11],[-L${x_libraries} -lX11 -lXext])
    VLC_ADD_CPPFLAGS([x11],[-I${x_includes}])
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
  CPPFLAGS="${CPPFLAGS_save} -I${x_includes}"
  AC_CHECK_HEADERS(X11/extensions/Xv.h, [
    CFLAGS="${CFLAGS_save} -L${x_libraries} -lX11 -lXext"
    AC_CHECK_LIB(Xv,XvPutImage,[
      # If libXv.so is available, xvideo can be a plugin. Otherwise, we
      # test for libXv_pic.
      if test -f /usr/X11R6/lib/libXv.so -o -f "${x_libraries}"/libXv.so; then
        VLC_ADD_PLUGINS([xvideo])
        VLC_ADD_CPPFLAGS([xvideo],[-I${x_includes}])
        VLC_ADD_LDFLAGS([xvideo],[-L${x_libraries} -lX11 -lXext -lXv])
      else
        AC_CHECK_LIB(Xv_pic,XvPutImage,[
          VLC_ADD_PLUGINS([xvideo])
          VLC_ADD_CPPFLAGS([xvideo],[-I${x_includes}])
          VLC_ADD_LDFLAGS([xvideo],[-L${x_libraries} -lX11 -lXext -lXv_pic])
        ],[
          VLC_ADD_BUILTINS([xvideo])
          VLC_ADD_LDFLAGS([xvideo],[-L${x_libraries} -lX11 -lXext -lXv])
          VLC_ADD_CPPFLAGS([xvideo],[-I${x_includes}])
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
  CPPFLAGS="${CPPFLAGS_save} -I${x_includes}"
  AC_CHECK_HEADERS(X11/Xlib.h, [
    AC_CHECK_HEADERS(GL/glx.h, [
      VLC_ADD_PLUGINS([glx])
      VLC_ADD_LDFLAGS([glx],[-L${x_libraries} -lX11 -lXext -lGL -lGLU])
      VLC_ADD_CPPFLAGS([glx],[-I${x_includes}])
  ]) ])
  CPPFLAGS="${CPPFLAGS_save}"
fi

dnl
dnl  Check for the Xinerama extension
dnl
if test "${enable_xvideo}" != "no" &&
  (test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" ||
   test "${enable_xvideo}" = "yes"); then
  ac_cv_have_xinerama="no"
  CPPFLAGS="${CPPFLAGS_save} -I${x_includes}"
  CFLAGS="${CFLAGS_save} -L${x_libraries} -lX11 -lXext"
  AC_CHECK_HEADERS(X11/extensions/Xinerama.h,[
    AC_CHECK_LIB(Xinerama_pic, XineramaQueryExtension,[
      VLC_ADD_LDFLAGS([xvideo],[-lXinerama_pic])
      VLC_ADD_LDFLAGS([x11],[-lXinerama_pic])
      VLC_ADD_LDFLAGS([glx],[-lXinerama_pic])
      ac_cv_have_xinerama="yes"
    ],[
      AC_CHECK_LIB(Xinerama, XineramaQueryExtension,[
        VLC_ADD_LDFLAGS([xvideo],[-lXinerama])
        VLC_ADD_LDFLAGS([x11],[-lXinerama])
        VLC_ADD_LDFLAGS([glx],[-lXinerama])
        ac_cv_have_xinerama="yes"
      ])
    ])
  ])
  if test "${ac_cv_have_xinerama}" = "yes"; then
    AC_DEFINE(HAVE_XINERAMA, 1, [Define this if you have libXinerama installed])
  fi
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
    AC_CHECK_HEADERS(GL/gl.h, [
      VLC_ADD_PLUGINS([opengl])
      if test "${SYS}" != "mingw32"; then
        VLC_ADD_LDFLAGS([opengl],[-L${x_libraries} -lGL -lGLU])
      else
        VLC_ADD_LDFLAGS([opengl],[-lopengl32])
      fi
    ])
  else
    dnl OS X special case (no GL/gl.h but OpenGL/gl.h)
    VLC_ADD_PLUGINS([opengl])
    VLC_ADD_LDFLAGS([opengl],[-framework OpenGL])
  fi
fi

dnl
dnl  SDL module
dnl
AC_ARG_ENABLE(sdl,
  [  --enable-sdl            SDL support (default enabled)])
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
    if test "${SYS}" != "darwin"; then
      VLC_ADD_PLUGINS([vout_sdl aout_sdl])
    fi
    VLC_ADD_CFLAGS([vout_sdl aout_sdl sdl_image],[`${SDL_CONFIG} ${SDL_PREFIX} --cflags | sed 's,SDL,,'`])
    VLC_ADD_LDFLAGS([vout_sdl aout_sdl sdl_image],[`${SDL_CONFIG} ${SDL_PREFIX} --libs | sed 's,-rdynamic,,'`])
    CPPFLAGS="${CPPFLAGS_save} ${CFLAGS_vout_sdl}"
    AC_CHECK_HEADERS(${SDL_HEADER}, AC_DEFINE_UNQUOTED(SDL_INCLUDE_FILE,
      <${SDL_HEADER}>, Indicate whether we should use SDL/SDL.h or SDL11/SDL.h),
      [ AC_CHECK_HEADERS(SDL.h, AC_DEFINE(SDL_INCLUDE_FILE, <SDL.h>,
          As a last resort we also test for SDL.h presence),
      [ AC_MSG_ERROR([The development package for SDL is not installed.
Please install it and try again. Alternatively you can also configure with
--disable-sdl.])
      ])])
    AC_CHECK_HEADERS(${SDL_IMAGE}, [AC_DEFINE_UNQUOTED(SDL_IMAGE_INCLUDE_FILE,
      <${SDL_IMAGE}>, Indicate the path of SDL_image.h)
      VLC_ADD_PLUGINS([sdl_image])
      VLC_ADD_LDFLAGS([sdl_image], [-lSDL_image])],
      [ AC_CHECK_HEADERS(SDL_image.h, AC_DEFINE(SDL_IMAGE_INCLUDE_FILE, <SDL_image.h>,
          As a last resort we also test for SDL_image.h presence),
      [ AC_MSG_WARN([The development package for SDL_image is not installed.
You should install it alongside your SDL package.])
      ])])
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
  FREETYPE_PATH="${PATH}"
  AC_ARG_WITH(freetype-config-path,
    [    --with-freetype-config-path=PATH freetype-config path (default search in \$PATH)],
    [ if test "${with_freetype_config_path}" != "no"
      then
        FREETYPE_PATH="${with_freetype_config_path}:${PATH}"
      fi ])
  AC_PATH_PROG(FREETYPE_CONFIG, freetype-config, no, ${FREETYPE_PATH})

  if test "${FREETYPE_CONFIG}" != "no"
  then
    VLC_ADD_PLUGINS([freetype])
    VLC_ADD_CFLAGS([freetype],[`${FREETYPE_CONFIG} --cflags`])
    VLC_ADD_LDFLAGS([freetype],[`${FREETYPE_CONFIG} --libs`])
    AC_CHECK_HEADERS(Carbon/Carbon.h,
      [VLC_ADD_LDFLAGS([freetype],[-framework Carbon])])
  elif test "${enable_freetype}" =  "yes"
  then
    AC_MSG_ERROR([I couldn't find the freetype package. You can download libfreetype2
from http://www.freetype.org/, or configure with --disable-freetype. Have a nice day.
    ])
  fi

  dnl fribidi support
  if test "${enable_fribidi}" != "no"
  then
    FRIBIDI_PATH="${PATH}"
    AC_ARG_WITH(fribidi-config-path,
      [    --with-fribidi-config-path=PATH fribidi-config path (default search in \$PATH)],
      [ if test "${with_fribidi_config_path}" != "no"
        then
          FRIBIDI_PATH="${with_fribidi_config_path}:${PATH}"
        fi ])
    AC_PATH_PROG(FRIBIDI_CONFIG, fribidi-config, no, ${FRIBIDI_PATH})

    if test "${FRIBIDI_CONFIG}" != "no"
    then
      VLC_ADD_CFLAGS([freetype], [`${FRIBIDI_CONFIG} --cflags` -DHAVE_FRIBIDI])
      VLC_ADD_CPPFLAGS([skins2], [`${FRIBIDI_CONFIG} --cflags` -DHAVE_FRIBIDI])
      VLC_ADD_LDFLAGS([freetype], [`${FRIBIDI_CONFIG} --libs`])
      VLC_ADD_LDFLAGS([skins2], [`${FRIBIDI_CONFIG} --libs`])
    fi
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
    VLC_ADD_LDFLAGS([xml],[`${XML2_CONFIG} --libs`])
    dnl depends on the xmlTextReader extension
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_xml}"
    LDFLAGS="${LDFLAGS_save} ${LDFLAGS_xml}"
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
if test "${enable_svg}" == "yes"
then
  PKG_CHECK_MODULES(SVG, 
	librsvg-2.0 >= 2.5.0,
	[
 	  VLC_ADD_LDFLAGS([svg],[$SVG_LIBS])
      	  VLC_ADD_CFLAGS([svg],[$SVG_CFLAGS])
          VLC_ADD_PLUGINS([svg]) ],
        [AC_MSG_WARN(SVG library not found)])
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
    VLC_ADD_LDFLAGS([qte],[-L${with_qte}/lib `echo -L${with_qte}/lib | sed 's,opt/QtPalmtop,usr,'` -ljpeg -lqte])
    VLC_ADD_CXXFLAGS([qte],[-I${with_qte}/include `echo -I${with_qte}/include | sed 's,opt/QtPalmtop,usr,'` -DQT_QWS_IPAQ -DQWS -fno-exceptions -fno-rtti])
  else
    VLC_ADD_LDFLAGS([qte],[-L${QTDIR}/lib `echo -L${QTDIR}/lib | sed 's,opt/QtPalmtop,usr,'` -ljpeg -lqte])
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
dnl  VLC_ADD_LDFLAGS([qt_video],[-L${QTDIR}/lib])
dnl  LDFLAGS="${LDFLAGS_save} ${LDFLAGS_qt_video}"
dnl   AC_CHECK_LIB(qt-mt,main,[
dnl    VLC_ADD_LDFLAGS([qt_video],[-lqt-mt])
dnl  ],[
dnl    AC_CHECK_LIB(qt,main,[
dnl      VLC_ADD_LDFLAGS([qt_video],[-lqt])
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
    VLC_ADD_LDFLAGS([hd1000v],[-lCascade -ldvbpsi -lmad])
  fi
  AC_LANG_POP([C++])
fi

dnl
dnl  Windows DirectX module
dnl
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
        VLC_ADD_LDFLAGS([vout_directx],[-lgdi32])
        dnl to be moved when dependance is removed
        AC_CHECK_HEADERS(GL/gl.h, [
            VLC_ADD_PLUGINS([glwin32])
            VLC_ADD_LDFLAGS([glwin32],[-lopengl32 -lgdi32])
        ]) ])
    else
      AC_MSG_CHECKING(for directX headers in ${with_directx})
      if test -f ${with_directx}/ddraw.h
      then
        VLC_ADD_PLUGINS([vout_directx aout_directx])
        VLC_ADD_CPPFLAGS([vout_directx aout_directx],[-I${with_directx}])
        VLC_ADD_LDFLAGS([vout_directx],[-lgdi32])
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
  VLC_ADD_LDFLAGS([svgalib],[-lvgagl -lvga])
fi

dnl
dnl  GGI module
dnl
AC_ARG_ENABLE(ggi,
  [  --enable-ggi            GGI support (default disabled)])
if test "${enable_ggi}" = "yes"
then
  VLC_ADD_PLUGINS([ggi])
  VLC_ADD_LDFLAGS([ggi],[-lggi])
  AC_ARG_WITH(ggi,
    [    --with-ggi=PATH       path to libggi],
    [ if test "${with_ggi}" != "no" -a -n "${with_ggi}"
      then
        VLC_ADD_CPPFLAGS([ggi],[-I${with_ggi}/include])
        VLC_ADD_LDFLAGS([ggi],[-L${with_ggi}/lib])
      fi ])
fi

dnl
dnl  Glide module
dnl
AC_ARG_ENABLE(glide,
  [  --enable-glide          Glide (3dfx) support (default disabled)])
if test "${enable_glide}" = "yes"
then
  VLC_ADD_PLUGINS([glide])
  VLC_ADD_LDFLAGS([glide],[-lglide2x -lm])
  VLC_ADD_CPPFLAGS([glide],[-I/usr/include/glide])
  AC_ARG_WITH(glide,
    [    --with-glide=PATH     path to libglide],
    [ if test "${with_glide}" != "no" -a -n "${with_glide}"
      then
        VLC_ADD_CPPFLAGS([glide],[-I${with_glide}/include])
        VLC_ADD_LDFLAGS([glide],[-L${with_glide}/lib])
      fi ])
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
    VLC_ADD_LDFLAGS([aa],[-laa])
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
    VLC_ADD_LDFLAGS([caca],[`${CACA_CONFIG} --plugin-libs`])
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
    VLC_ADD_LDFLAGS([wingdi],[-lgdi32])
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
    AC_CHECK_LIB(ossaudio,main,VLC_ADD_LDFLAGS([oss],[-lossaudio]))
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
       VLC_ADD_LDFLAGS([esd],[`${ESD_CONFIG} --libs`])
     fi
   fi])

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
        VLC_ADD_LDFLAGS([portaudio],[-lportaudio -lwinmm -lole32])
     else
        VLC_ADD_LDFLAGS([portaudio],[-lportaudio])
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
      VLC_ADD_LDFLAGS([arts],[`${ARTS_CONFIG} --libs `])
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
    AC_TRY_COMPILE([#define ALSA_PCM_NEW_HW_PARAMS_API
                    #define ALSA_PCM_NEW_SW_PARAMS_API
                    #include <alsa/asoundlib.h>],
       [void foo() { snd_pcm_hw_params_get_period_time(0,0,0); }],
        AC_DEFINE(HAVE_ALSA_NEW_API, 1, Define if ALSA is at least rc4))
    VLC_ADD_PLUGINS([alsa])
    VLC_ADD_LDFLAGS([alsa],[-lasound -lm -ldl])
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
    VLC_ADD_LDFLAGS([waveout],[-lwinmm])
  fi
  if test "${SYS}" = "mingwce"; then
    VLC_ADD_PLUGINS([waveout])
  fi
fi

dnl
dnl  CoreAudio plugin
dnl
AC_ARG_ENABLE(coreaudio,
  [  --enable-coreaudio      CoreAudio module (default enabled on MacOS X)])
if test "${enable_coreaudio}" != "no" &&
  (test "${SYS}" = "darwin" || test "${enable_coreaudio}" = "yes")
then
  AC_CHECK_HEADERS(CoreAudio/CoreAudio.h, 
    [ VLC_ADD_BUILTINS([coreaudio auhal])
      VLC_ADD_LDFLAGS([coreaudio auhal],[-framework CoreAudio -framework AudioUnit -framework AudioToolbox])
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
    AC_CHECK_LIB(HDMachineX225,main,VLC_ADD_LDFLAGS([hd1000a],[-lHDMachineX225]))  ])
  AC_LANG_POP([C++])
fi

dnl
dnl  CyberLink for C++ UPnP stack
dnl
AC_ARG_ENABLE(cyberlink,
  [  --enable-cyberlink      CyberLink for C++ UPnP stack (default disabled)])
if test "${CXX}" != "" -a "${enable_cyberlink}" = "yes" || (test "${enable_cyberlink}" != "no"); then
  AC_ARG_WITH(cyberlink-tree,
    [    --with-cyberlink-tree=PATH CyberLink for C++ tree for static linking])

  dnl
  dnl test for --with-cyberlink-tree
  dnl
  if test ! -z "${with_cyberlink_tree}" -a "${CXX}" != ""; then
    AC_LANG_PUSH(C++)
    real_cyberlink_tree="`cd ${with_cyberlink_tree} 2>/dev/null && pwd`"
    if test -z "${real_cyberlink_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_cyberlink_tree}])
    fi
    CXXFLAGS_save="${CXXFLAGS}"
    CXXFLAGS_cyberlink="-I${real_cyberlink_tree}/include"
    CXXFLAGS="${CXXFLAGS} ${CXXFLAGS_cyberlink}"
    AC_CHECK_HEADERS([cybergarage/upnp/MediaServer.h],
      [ VLC_ADD_CXXFLAGS([upnp], [${CXXFLAGS_cyberlink}])
        VLC_ADD_PLUGINS([upnp]) 
      ],[
        AC_MSG_ERROR([cannot find CyberLink for C++ headers])
      ])
    AC_MSG_CHECKING(for libclink.a in ${with_cyberlink_tree})
    if test -f "${real_cyberlink_tree}/lib/unix/libclink.a"
    then
      AC_MSG_RESULT(${real_cyberlink_tree}/lib/unix/libclink.a)
      VLC_ADD_LDFLAGS([upnp], [${real_cyberlink_tree}/lib/unix/libclink.a -lexpat])
    else
      AC_MSG_RESULT(no)
       AC_MSG_ERROR([cannot find ${real_cyberlink_tree}/lib/unix/libclink.a, make sure you compiled CyberLink for C++ in ${with_cyberlink_tree}])
    fi
    CXXFLAGS="${CXXFLAGS_save}"
    AC_LANG_POP([C++])
  fi
fi

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
  [  --enable-skins2         Skins2 interface module (experimental)])
if test "${enable_skins2}" = "yes" ||
  (test "${SYS}" != "darwin" && test "${SYS}" != "beos" &&
   test "${SYS}" != "mingwce" && test "${enable_skins2}" != "no"); then

  dnl test for the required libraries
  skins2_missing_lib="no"

  dnl freetype
  if test "${FREETYPE_CONFIG}" != "no"; then
    VLC_ADD_CPPFLAGS([skins2],[`${FREETYPE_CONFIG} --cflags`])
    VLC_ADD_LDFLAGS([skins2],[`${FREETYPE_CONFIG} --libs`])
  else
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
    VLC_ADD_LDFLAGS([skins2],[-loleaut32 -lwinspool -lwinmm -lshell32 -lctl3d32 -ladvapi32 -lwsock32 -lgdi32 -lcomdlg32 -lole32 -luuid -lcomctl32])

  else if test "${skins2_missing_lib}" = "no"; then
    VLC_ADD_PLUGINS([skins2])
    ALIASES="${ALIASES} svlc"
    VLC_ADD_CPPFLAGS([skins2],[-Imodules/gui/skins2 -I${x_includes} -DX11_SKINS])
    VLC_ADD_CXXFLAGS([skins2],[-O2 -fno-rtti])
    VLC_ADD_LDFLAGS([skins2],[-L${x_libraries} -lXext -lX11])
  fi fi
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
dnl       VLC_ADD_LDFLAGS([gtk],[`${GTK_CONFIG} --libs gtk gthread | sed 's,-rdynamic,,'`])
dnl     else
dnl       VLC_ADD_CFLAGS([gtk],[`${GTK_CONFIG} --cflags gtk`])
dnl       VLC_ADD_LDFLAGS([gtk],[`${GTK_CONFIG} --libs gtk | sed 's,-rdynamic,,'`])
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
dnl   VLC_ADD_LDFLAGS([gtk2],[${GTK2_LIBS}])
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
  VLC_ADD_LDFLAGS([gtk2],[${GTK2_LIBS}])
  VLC_ADD_CFLAGS([pda],[${GTK2_CFLAGS} ${CFLAGS_pda}])
  VLC_ADD_LDFLAGS([pda],[${GTK2_LIBS} ${LDFLAGS_pda}])
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
dnl        VLC_ADD_LDFLAGS([gnome],[`${GNOME_CONFIG} --libs gnomeui | sed 's,-rdynamic,,'`])
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
dnl   VLC_ADD_LDFLAGS([gnome2],[${GNOME2_LIBS}])
dnl   VLC_ADD_PLUGINS([gnome2])
dnl   if test "${SYS}" != "mingw32"; then
dnl     NEED_GNOME2_MAIN=yes
dnl   fi
dnl fi

dnl
dnl  wxWidgets module
dnl
AC_ARG_ENABLE(wxwidgets,
  [  --enable-wxwidgets      wxWidgets support (default enabled)])
if test "${enable_wxwindows}" 
then
  AC_MSG_WARN(--{en|dis}able-wxwindows is deprecated. Use --{en|dis}able-wxwidgets instead.)
fi

if test "${enable_wxwindows}" = "no"
then
  enable_wxwidgets="no"
fi
if test "${enable_wxwidgets}" != "no"
then
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
    if expr 2.3.0 \> `${WX_CONFIG} --version` >/dev/null
    then
      AC_MSG_ERROR([Your development package for wxWidgets is too old, you need at least version 2.3.0. Please upgrade and try again. Alternatively you can also configure with --disable-wxwidgets.])
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
    VLC_ADD_LDFLAGS([wxwidgets],[`${WX_CONFIG} --libs`])
    VLC_ADD_CXXFLAGS([wxwidgets],[`${WX_CONFIG} --cxxflags`])
    if test "$have_libcdio" = "yes"
    then 
      VLC_ADD_LDFLAGS([wxwidgets],[$LIBCDIO_LIBS])
      VLC_ADD_CXXFLAGS([wxwidgets],[$LIBCDIO_CFLAGS])
    else 
      AC_MSG_WARN([Probe disc disabled because ok libcdio library not found])
    fi

    if test "$have_libvcdinfo" = "yes"
    then 
      VLC_ADD_LDFLAGS([wxwidgets],[$VCDINFO_LIBS])
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
      ALIASES="${ALIASES} wxvlc"
    fi
    CPPFLAGS="${CPPFLAGS_save}"
    AC_LANG_POP(C++)
  fi
fi

dnl
dnl  WinCE GUI module
dnl
if test "${SYS}" = "mingwce"; then
  VLC_ADD_BUILTINS([wince])
  VLC_ADD_CXXFLAGS([wince],[])
  VLC_ADD_LDFLAGS([wince],[-lcommctrl -lcommdlg -laygshell])
  dnl Gross hack
  VLC_ADD_LDFLAGS([wince],[\\\${top_builddir}modules/gui/wince/wince_rc.o])
elif test "${SYS}" = "mingw32"; then
  VLC_ADD_CXXFLAGS([wince],[])
  VLC_ADD_LDFLAGS([wince],[-lcomctl32 -lcomdlg32 -lgdi32 -lole32])
  dnl Gross hack
  VLC_ADD_LDFLAGS([wince],[\\\${top_builddir}modules/gui/wince/wince_rc.o])
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
dnl      VLC_ADD_LDFLAGS([qt],[-L${QTDIR}/lib])
dnl      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_qt}"
dnl      AC_CHECK_LIB(qt-mt,main,[
dnl        VLC_ADD_LDFLAGS([qt],[-lqt-mt])
dnl      ],[
dnl        AC_CHECK_LIB(qt,main,[
dnl          VLC_ADD_LDFLAGS([qt],[-lqt])
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
dnl      VLC_ADD_LDFLAGS([kde],[-L${KDEDIR}/lib])
dnl      dnl Check for -lkfile (only in KDE 2) or -lkdeui -lkio (KDE 3)
dnl      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_kde}"
dnl      AC_CHECK_LIB(kfile,main,[
dnl        VLC_ADD_LDFLAGS([kde],[-lkfile])
dnl      ])
dnl      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_kde}"
dnl      AC_CHECK_LIB(kdeui,main,[
dnl        VLC_ADD_LDFLAGS([kde],[-lkdeui])
dnl      ])
dnl      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_kde}"
dnl      AC_CHECK_LIB(kio,main,[
dnl        VLC_ADD_LDFLAGS([kde],[-lkio])
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
       VLC_ADD_LDFLAGS([qte],[-L${with_qte}/lib `echo -L${with_qte}/lib | sed 's,opt/QtPalmtop,usr,'` -ljpeg -lqte])
       VLC_ADD_CXXFLAGS([qte],[-I${with_qte}/include `echo -I${with_qte}/include | sed 's,opt/QtPalmtop,usr,'` -DQT_QWS_IPAQ -DQWS -fno-exceptions -fno-rtti])
     else
       VLC_ADD_LDFLAGS([qte],[-L${QTDIR}/lib `echo -L${QTDIR}/lib | sed 's,opt/QtPalmtop,usr,'`])
       VLC_ADD_CXXFLAGS([qte],[-I${QTDIR}/include `echo -I${QTDIR}/include | sed 's,opt/QtPalmtop,usr,'`])
     fi
     CPPFLAGS="${CPPFLAGS_save} ${CXXFLAGS_qte}"
     AC_CHECK_HEADERS(qt.h jpeglib.h, ,[
       AC_MSG_ERROR([echo "Cannot find QT Embedded development headers."])
     ] )
     CPPFLAGS="${CPPFLAGS_save}"

     VLC_ADD_PLUGINS([opie])
     NEED_QTE_MAIN=yes
     VLC_ADD_LDFLAGS([opie],[-lqpe ${LDFLAGS_qte}])
     VLC_ADD_CXXFLAGS([opie],[${CXXFLAGS_qte}])
     if test "${with_qte}" != "no" -a -n "${with_qte}"
     then
       MOC=${with_qte}/bin/moc
     else
       MOC=${QTDIR}/bin/moc
     fi
   fi])

dnl
dnl  MacOS X module
dnl
AC_ARG_ENABLE(macosx,
  [  --enable-macosx         MacOS X support (default enabled on MacOS X)],
  [if test "${enable_macosx}" = "yes"
   then
     VLC_ADD_BUILTINS([macosx])
     VLC_ADD_LDFLAGS([macosx],[-framework IOKit -framework Cocoa -framework Carbon -framework QuickTime -lobjc -ObjC -framework OpenGL])
   fi],
  [AC_CHECK_HEADERS(Cocoa/Cocoa.h,
     VLC_ADD_BUILTINS([macosx])
     VLC_ADD_LDFLAGS([macosx],[-framework IOKit -framework Cocoa -framework Carbon -framework QuickTime -lobjc -ObjC -framework OpenGL])
   )])

dnl
dnl  QNX RTOS module
dnl
AC_ARG_ENABLE(qnx,
  [  --enable-qnx            QNX RTOS support (default enabled on QNX RTOS)])
    if test "${enable_qnx}" != "no"
    then
      AC_CHECK_HEADERS(Ph.h, [
        VLC_ADD_PLUGINS([qnx])
        VLC_ADD_LDFLAGS([qnx],[-lasound -lph])
      ])
    fi

dnl
dnl  ncurses module
dnl
AC_ARG_ENABLE(ncurses,
  [  --enable-ncurses        ncurses interface support (default disabled)],
  [if test "${enable_ncurses}" = "yes"; then
     VLC_ADD_PLUGINS([ncurses])
     VLC_ADD_LDFLAGS([ncurses],[-lncurses])
   fi])

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
         [void foo() { xosd_init("foo","bar",12,XOSD_top,2,12,42); }],,
          AC_DEFINE(HAVE_XOSD_VERSION_0, 1, Define if <xosd.h> is pre-1.0.0))))
  if test "${have_xosd}" = "true"
  then
    VLC_ADD_PLUGINS([xosd])
    VLC_ADD_LDFLAGS([xosd],[-lxosd])
  fi
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
  AC_CHECK_HEADERS(GL/gl.h, [
    VLC_ADD_PLUGINS([galaktos])
    if test "${SYS}" != "mingw32"; then
      VLC_ADD_LDFLAGS([galaktos],[-L${x_libraries} -lGL -lGLU])
    else
      VLC_ADD_LDFLAGS([galaktos],[-lopengl32])
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
      VLC_ADD_LDFLAGS([goom],[-L${real_goom_tree}/src/.libs -lgoom2])
      VLC_ADD_CPPFLAGS([goom],[-I${real_goom_tree}/src -DUSE_GOOM_TREE])
    else
      dnl  The given libgoom2 wasn't built, try to look for the old goom
      AC_MSG_RESULT(no)
      AC_MSG_CHECKING(for libgoom.a in ${with_goom_tree})
      if test -f "${real_goom_tree}/libgoom.a"; then
        AC_MSG_RESULT(${real_goom_tree}/libgoom.a)
        VLC_ADD_BUILTINS([goom])
        VLC_ADD_LDFLAGS([goom],[-L${real_goom_tree} -lgoom])
        VLC_ADD_CPPFLAGS([goom],[-I${real_goom_tree} -DUSE_GOOM_TREE -DOLD_GOOM])
      else
        dnl  The given libgoom wasn't built
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot find ${real_goom_tree}/src/.libs/libgoom2.a, make sure you compiled goom in ${with_goom_tree}])
      fi
    fi
  else
    AC_CHECK_HEADERS(goom/goom.h, [
      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_goom}"
      AC_CHECK_LIB(goom2, goom_init, [
        VLC_ADD_PLUGINS([goom])
        VLC_ADD_LDFLAGS([goom],[-lgoom2])
      ],[
        AC_MSG_ERROR([Could not find goom on your system: you may get it from http://www.ios-software.com/.])
      ])
      LDFLAGS="${LDFLAGS_save}"
    ])
  fi
fi

dnl
dnl  SLP access plugin
dnl
AC_ARG_ENABLE(slp,
  [  --enable-slp            SLP service discovery support (default disabled)])
if test "${enable_slp}" = "yes"
then
  AC_ARG_WITH(slp,
  [    --with-slp=PATH       libslp headers and libraries])
  if test -z "${with_slp}"
  then
    AC_CHECK_HEADERS(slp.h, have_slp="true", have_slp="false")
    if test "${have_slp}" = "true"
    then
      VLC_ADD_PLUGINS([slp])
      VLC_ADD_LDFLAGS([slp],[-lslp])
      VLC_ADD_LDFLAGS([stream_out_standard],[-lslp])
    fi
  else
    AC_MSG_CHECKING(for slp headers in ${with_slp})
    if test -f ${with_slp}/slp.h
    then
      dnl  Use ${with_slp}/libslp/slp.h
      AC_MSG_RESULT(yes)
      VLC_ADD_PLUGINS([slp])
      VLC_ADD_LDFLAGS([slp],[-L${with_slp} -lslp])
      VLC_ADD_LDFLAGS([stream_out_standard],[-L${with_slp} -lslp])
      VLC_ADD_CPPFLAGS([slp],[-I${with_slp}])
      AC_DEFINE(HAVE_SLP_H)
    else
      dnl  No libslp could be found, sorry
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${with_slp}/slp.h])
    fi
  fi
fi

dnl
dnl DAAP access plugin and services discovery 
dnl
AC_ARG_ENABLE(daap,
  [  --enable-daap	          DAAP shares services discovery support (default enabled)])
if test "$enable_daap" != "no"
then
   PKG_CHECK_MODULES(DAAP, opendaap >= 0.3.0,
      [ VLC_ADD_PLUGINS([daap])
       VLC_ADD_LDFLAGS([daap],[$DAAP_LIBS])
       VLC_ADD_CFLAGS([daap],[$DAAP_CFLAGS])]:,
      [AC_MSG_WARN(DAAP library not found)])
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
    VLC_ADD_LDFLAGS([lirc],[-llirc_client])
  fi
fi

dnl 
dnl  Joystick plugin
dnl
AC_ARG_ENABLE(joystick,
  [  --enable-joystick       joystick control (default enabled)])
if test "${enable_joystick}" = "yes"; then
  AC_CHECK_HEADER(linux/joystick.h, [VLC_ADD_PLUGINS([joystick])])
fi

dnl
dnl corba (ORBit) plugin
dnl
dnl Default: do not enable corba
enablecorba=false
AC_ARG_ENABLE(corba,
  [  --enable-corba          corba interface support (default disabled)])
if test "${enable_corba}" = "yes"; then
      GLIB_VERSION=2.3.2
      PKG_CHECK_MODULES(CORBA, 
	ORBit-2.0 >= 2.8.0 \
	glib-2.0 >= $GLIB_VERSION \
	gobject-2.0 >= $GLIB_VERSION \
	gthread-2.0 >= $GLIB_VERSION,
	[
	  enablecorba=true
 	  VLC_ADD_LDFLAGS([corba],[$CORBA_LIBS])
      	  VLC_ADD_CFLAGS([corba],[$CORBA_CFLAGS])
          VLC_ADD_PLUGINS([corba snapshot]) ],
        [ enablecorba=false
	  AC_MSG_WARN(corba library not found) ])
fi
AM_CONDITIONAL(ENABLE_CORBA, test "$enablecorba" = "true")

AC_ARG_WITH(,[Misc options:])

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
  [ VLC_ADD_PLUGINS([dmo quicktime])
    VLC_ADD_CPPFLAGS([dmo],[-I../../../@top_srcdir@/loader])
    VLC_ADD_LDFLAGS([dmo],[../../../loader/libloader.a])
    VLC_ADD_CPPFLAGS([quicktime],[-I../../@top_srcdir@/loader])
    VLC_ADD_LDFLAGS([quicktime],[../../loader/libloader.a])
  ])

dnl
dnl  Microsoft ActiveX support
dnl
activex=false
AC_ARG_ENABLE(activex,
  [  --enable-activex        build a vlc-based ActiveX control (default enabled on Win32)])
if test "${enable_activex}" != "no"
then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"
  then
    AC_CHECK_PROGS(MIDL, [midl widl], no)
    AC_CHECK_HEADERS(ole2.h olectl.h,
      [ VLC_ADD_CXXFLAGS([activex],[-fno-exceptions])
        VLC_ADD_LDFLAGS([activex],[-lole32 -loleaut32 -luuid -lshlwapi]) 
        AC_CHECK_HEADERS(objsafe.h,
          VLC_ADD_CXXFLAGS([activex],[-DHAVE_OBJSAFE_HEADER])
        )
        activex=:
      ],
      [ AC_MSG_ERROR([required OLE headers are missing from your system]) ]
    )
  fi
fi
AC_ARG_VAR(MIDL, [Microsoft IDL compiler (Win32 platform only)])
AM_CONDITIONAL(HAS_MIDL_COMPILER, test "${MIDL}" != "no")
AM_CONDITIONAL(BUILD_ACTIVEX,${activex})

dnl
dnl  Mozilla plugin
dnl
mozilla=false
AC_ARG_ENABLE(mozilla,
  [  --enable-mozilla        build a vlc-based Mozilla plugin (default disabled)])
if test "${enable_mozilla}" = "yes" -a "${SYS}" != "mingw32"
then
  AC_PATH_PROG(MOZILLA_CONFIG, mozilla-config, no)
  if test "${MOZILLA_CONFIG}" = "no"
  then
    AC_MSG_ERROR([Please install the Mozilla development tools, mozilla-config was not found.])
  else
    if test "${SYS}" != "mingw32"; then
      LDFLAGS="${LDFLAGS_save} -L${x_libraries}"
      AC_CHECK_LIB(Xt,XtStrings,
       [VLC_ADD_LDFLAGS([mozilla],[-L${x_libraries} -lXt -lX11 -lSM -lICE])],
       [],
       [[-L${x_libraries} -lX11 -lSM -lICE]
      ])
      LDFLAGS="${LDFLAGS_save}"
    fi
    mozilla=:
    dnl Workaround for http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=150490
    VLC_ADD_CPPFLAGS([mozilla],[[`${MOZILLA_CONFIG} --cflags plugin xpcom java | sed 's,-I\([^ ]*\)/mozilla/\([^ ]*\),-I\1/\2 -I\1/mozilla/\2,g' | xargs`]])
    VLC_ADD_LDFLAGS([mozilla],[`${MOZILLA_CONFIG} --libs plugin xpcom`])
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_mozilla}"
    AC_CHECK_HEADERS(mozilla-config.h)
    CPPFLAGS="${CPPFLAGS_save}"
  fi

dnl special case for mingw32
elif test "${enable_mozilla}" = "yes"
then
  AC_CHECK_TOOL(CYGPATH, cygpath, "")
  AC_ARG_WITH(mozilla-sdk-path,
    [    --with-mozilla-sdk-path=PATH path to win32 mozilla sdk], [
    real_mozilla_sdk="`cd ${with_mozilla_sdk_path} 2>/dev/null && pwd`"
    CPPFLAGS="${CPPFLAGS_save} ${real_mozilla_sdk}"
    AC_CHECK_HEADERS(mozilla-config.h, [
      mozilla=:
      VLC_ADD_CPPFLAGS([mozilla],[-DXPCOM_GLUE -I${real_mozilla_sdk} -I${real_mozilla_sdk}/embedstring/include -I${real_mozilla_sdk}/xpcom/include -I${real_mozilla_sdk}/nspr/include -I${real_mozilla_sdk}/string/include -I${real_mozilla_sdk}/plugin/include -I${real_mozilla_sdk}/java/include])
      VLC_ADD_LDFLAGS([mozilla],[-L${real_mozilla_sdk}/embedstring/bin -L${real_mozilla_sdk}/xpcom/bin -L${real_mozilla_sdk}/nspr/bin -L${real_mozilla_sdk}/string/bin -lnspr4 -lplds4 -lplc4 -lembedstring -lxpcomglue -Wl,--kill-at])
      XPIDL_INCL="-I${real_mozilla_sdk}/xpcom/idl"
      if test -n "${CYGPATH}"; then
        XPIDL="${real_mozilla_sdk}/xpcom/bin/xpidl"
        real_mozilla_sdk="`${CYGPATH} -w ${real_mozilla_sdk}`"
        XPIDL_INCL="${XPIDL_INCL} -I\"${real_mozilla_sdk}/xpcom/idl\""
      fi ])
    CPPFLAGS="${CPPFLAGS_save}"
  ])
fi

dnl Not necessarily in ${PATH}
if test -z "${XPIDL}" -o ! -x "${XPIDL}"; then
  XPIDL="/usr/lib/mozilla/xpidl"
fi
AS_IF([test "${MOZILLA_CONFIG}"], [
  if test -z "${XPIDL_INCL}"; then
    XPIDL_INCL="`${MOZILLA_CONFIG} --cflags plugin xpcom java` \
    `${MOZILLA_CONFIG} --idlflags plugin xpcom java` "
  fi
])
AM_CONDITIONAL(BUILD_MOZILLA,${mozilla})

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
  VLC_ADD_LDFLAGS([test3],[-lobjc])

  VLC_ADD_PLUGINS([${TESTS}])
  #VLC_ADD_BUILTINS([${TESTS}])
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
builtin_support=false
plugin_support=:

dnl Support for plugins - this must be AT THE END
AC_ARG_ENABLE(plugins,
  [  --disable-plugins       make all plugins built-in (default plugins enabled)],
  [if test "${enable_plugins}" = "no"
   then
     plugin_support=false
   fi])

dnl Automagically disable plugins if there is no system support for
dnl dynamically loadable files (.so, .dll, .dylib).
dnl don't forget vlc-win32 still can load .dll as plugins
if test "${ac_cv_have_plugins}" = "no"
then
  echo "*** Your system doesn't have plugin support. All plugins will be built"
  echo "statically."
  plugin_support=false
fi

dnl Export automake variables
if ${plugin_support}
then
  AC_DEFINE(HAVE_DYNAMIC_PLUGINS, 1, Define if we have support for dynamic plugins)
  for plugin in `echo ${PLUGINS}`
  do
    eval "${plugin}_p=yes"
  done
else
  VLC_ADD_BUILTINS([${PLUGINS}])
  PLUGINS=""
fi
AM_CONDITIONAL(HAVE_PLUGINS, ${plugin_support})

[if echo "${BUILTINS}" | grep '[^ ]' >/dev/null 2>&1
then
  builtin_support=:
  for builtin in `echo ${BUILTINS}`
  do
    eval "${builtin}_b=yes"
  done
fi]
AM_CONDITIONAL(HAVE_BUILTINS, ${builtin_support})

dnl Import conditional variables generated by bootstrap
VLC_CONDITIONALS

dnl
dnl  Stuff used by the program
dnl
AC_DEFINE_UNQUOTED(VERSION_MESSAGE, "${VERSION} ${CODENAME}", [Simple version string])
AC_DEFINE_UNQUOTED(COPYRIGHT_MESSAGE, "VLC media player - version ${VERSION} ${CODENAME} - (c) 1996-2005 the VideoLAN team", [Copyright string])
AC_DEFINE_UNQUOTED(CONFIGURE_LINE, "${CONFIGURE_LINE}", [The ./configure command line])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_MAJOR,"${VERSION_MAJOR}", [version major number])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_MINOR,"${VERSION_MINOR}", [version minor number])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_REVISION,"${VERSION_REVISION}", [version minor number])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_EXTRA,"${VERSION_EXTRA}", [version minor number])
AC_SUBST(VERSION_MAJOR)
AC_SUBST(VERSION_MINOR)
AC_SUBST(VERSION_REVISION)

dnl Old definitions for version-dependant plugins
dnl VLC_SYMBOL="`echo ${VERSION} | sed -e 'y/.-+/___/'`"
dnl AC_DEFINE_UNQUOTED(MODULE_SUFFIX, "__${VLC_SYMBOL}", [String suffix for module functions])
dnl AC_DEFINE_UNQUOTED(MODULE_SYMBOL, ${VLC_SYMBOL}, [Symbol suffix for module functions])

dnl New definitions with value matching 0.8.2 release
AC_DEFINE_UNQUOTED(MODULE_SUFFIX, "__0_8_2", [String suffix for module functions])
AC_DEFINE_UNQUOTED(MODULE_SYMBOL, 0_8_2, [Symbol suffix for module functions])

DATA_PATH="${ac_tool_prefix}/share/vlc"
AC_SUBST(DATA_PATH)
PLUGIN_PATH="${ac_tool_prefix}/lib/vlc"
AC_SUBST(PLUGIN_PATH)

dnl
dnl  Handle substvars that use $(top_srcdir)
dnl
VLC_CONFIG="top_builddir=\"\$(top_builddir)\" \$(top_builddir)/vlc-config"
AC_SUBST(VLC_CONFIG)
CPPFLAGS_save="${CPPFLAGS_save} -I\$(top_srcdir)/include"

dnl
dnl  Restore *FLAGS
dnl
VLC_RESTORE_FLAGS

dnl
dnl  Create the vlc-config script
dnl
LDFLAGS_libvlc="${LDFLAGS_vlc} ${LDFLAGS_builtin}"
for i in `echo "${BUILTINS}" | sed -e 's@[^ ]*/@@g'` ; do LDFLAGS_libvlc="${LDFLAGS_libvlc} ${libdir}/vlc/${i}.a `eval echo '$'{LDFLAGS_${i}}`" ; done

dnl
dnl  Configuration is finished
dnl
AC_SUBST(SYS)
AC_SUBST(ARCH)
AC_SUBST(ALIASES)
AC_SUBST(ASM)
AC_SUBST(MOC)
AC_SUBST(WINDRES)
AC_SUBST(XPIDL)
AC_SUBST(XPIDL_INCL)
AC_SUBST(LIBEXT)
AC_SUBST(INCLUDES)
AC_SUBST(ALL_LINGUAS)

dnl Import substitutions generated by bootstrap
VLC_SUBSTS

dnl Create vlc-config.in
VLC_OUTPUT_VLC_CONFIG_IN

AC_CONFIG_FILES([
  Makefile
  activex/Makefile
  activex/axvlc.inf
  debian/Makefile
  doc/Makefile
  intl/Makefile
  ipkg/Makefile
  lib/Makefile
  loader/Makefile
  modules/Makefile
  mozilla/Makefile
  m4/Makefile
  po/Makefile.in
  share/Makefile
  src/Makefile
])

AC_CONFIG_FILES([
  modules/access/Makefile
  modules/access/dshow/Makefile
  modules/access/dvb/Makefile
  modules/access/mms/Makefile
  modules/access/pvr/Makefile
  modules/access/v4l/Makefile
  modules/access/cdda/Makefile
  modules/access/vcd/Makefile
  modules/access/vcdx/Makefile
  modules/access/screen/Makefile
  modules/access_filter/Makefile
  modules/access_output/Makefile
  modules/audio_filter/Makefile
  modules/audio_filter/channel_mixer/Makefile
  modules/audio_filter/converter/Makefile
  modules/audio_filter/resampler/Makefile
  modules/audio_mixer/Makefile
  modules/audio_output/Makefile
  modules/codec/Makefile
  modules/codec/cmml/Makefile
  modules/codec/dmo/Makefile
  modules/codec/ffmpeg/Makefile
  modules/codec/spudec/Makefile
  modules/control/Makefile
  modules/control/corba/Makefile
  modules/demux/Makefile
  modules/demux/asf/Makefile
  modules/demux/avi/Makefile
  modules/demux/mp4/Makefile
  modules/demux/mpeg/Makefile
  modules/demux/playlist/Makefile
  modules/demux/util/Makefile
  modules/gui/Makefile
  modules/gui/beos/Makefile
  modules/gui/pda/Makefile
  modules/gui/macosx/Makefile
  modules/gui/qnx/Makefile
  modules/gui/skins2/Makefile
  modules/gui/wxwidgets/Makefile
  modules/gui/wince/Makefile
  modules/misc/Makefile
  modules/misc/dummy/Makefile
  modules/misc/memcpy/Makefile
  modules/misc/network/Makefile
  modules/misc/testsuite/Makefile
  modules/misc/playlist/Makefile
  modules/misc/xml/Makefile
  modules/mux/Makefile
  modules/mux/mpeg/Makefile
  modules/packetizer/Makefile
  modules/services_discovery/Makefile
  modules/stream_out/Makefile
  modules/stream_out/transrate/Makefile
  modules/video_chroma/Makefile
  modules/video_filter/Makefile
  modules/video_filter/swscale/Makefile
  modules/video_output/Makefile
  modules/video_output/directx/Makefile
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

You can tune the compiler flags in vlc-config.
To build vlc and its plugins, type \`make'.
"

