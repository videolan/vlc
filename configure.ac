dnl Autoconf settings for vlc
dnl $Id: configure.ac,v 1.39 2003/07/21 17:48:31 gbazin Exp $

AC_INIT(vlc,0.6.1-test1)

CONFIGURE_LINE="$0 $*"
CODENAME="Trevelyan"

AC_PREREQ(2.50)
AC_CONFIG_SRCDIR(src/libvlc.c)
AC_CONFIG_AUX_DIR(autotools)
AC_CANONICAL_SYSTEM

dnl XXX: we don't put any flags here, because automake 1.5 doesn't support
dnl them. And we need the comma otherwize automake will choke on it.
AM_INIT_AUTOMAKE(vlc,0.6.1-test1)
AM_CONFIG_HEADER(config.h)

dnl
dnl  Save *FLAGS
dnl
AX_SAVE_FLAGS

dnl
dnl Check for tools
dnl
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_MAKE_SET
AC_PROG_INSTALL

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

dnl AM_PROG_LIBTOOL
AC_PROG_INSTALL

dnl Check for compiler properties
AC_C_CONST
AC_C_INLINE

dnl
dnl  Set default values
dnl
LDFLAGS_vlc="${LDFLAGS}"
LIBEXT=".so"

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
    AX_ADD_LDFLAGS([dvd dvdcss vcd cdda],[-ldvd])
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
    AX_ADD_LDFLAGS([vlc ffmpeg],[-all_load])
    AX_ADD_LDFLAGS([vlc],[-Wl,-multiply_defined,suppress])
    LIBEXT=".dylib"
    ;;
  *mingw32* | *cygwin*)
    AC_CHECK_TOOL(WINDRES, windres, :)
    LIBEXT=".dll"

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
    esac

    if test "${SYS}" = "mingw32"; then
        # add ws2_32 for closesocket, select, recv
        CPPFLAGS_save="${CPPFLAGS_save} -D_OFF_T_ -D_off_t=long"; CPPFLAGS="${CPPFLAGS_save}"
        AX_ADD_LDFLAGS([vlc],[-lws2_32 -lnetapi32 -lwinmm -mwindows])
        AX_ADD_LDFLAGS([ipv4 ipv6 access_http access_mms access_udp access_ftp access_output_udp sap slp http httpd],[-lws2_32])
    fi
    ;;
  *nto*)
    SYS=nto
    AX_ADD_LDFLAGS([x11 xvideo],[-lsocket])
    ;;
  solaris*)
    SYS=solaris
    ;;
  hpux*)
    SYS=hpux
    LIBEXT=".sl"
    ;;
  beos)
    SYS=beos
    CFLAGS_save="${CFLAGS_save} -Wno-multichar"; CFLAGS="${CFLAGS_save}"
    CXXFLAGS_save="${CXXFLAGS_save} -Wno-multichar"; CXXFLAGS="${CXXFLAGS_save}"
    AX_ADD_LDFLAGS([vlc beos],[-lbe])
    AX_ADD_CXXFLAGS([beos],[])
    AX_ADD_LDFLAGS([beos],[-lmedia -ltranslation -ltracker -lgame])
    dnl Check if we have BONE (old net_server doesn't have libsocket.so)
    AC_CHECK_LIB(socket, connect,
        AX_ADD_LDFLAGS([vlc access_mms ipv4 httpd],[-lsocket -lbind]))
    ;;
  *)
    SYS="${target_os}"
    ;;
esac
AM_CONDITIONAL(HAVE_BEOS, test "${SYS}" = "beos")
AM_CONDITIONAL(HAVE_DARWIN, test "${SYS}" = "darwin")
AM_CONDITIONAL(HAVE_WIN32, test "${SYS}" = "mingw32")

dnl
dnl Gettext stuff
dnl
ALL_LINGUAS="de en_GB fr it ja no ru nl pl sv"
AM_GNU_GETTEXT_VERSION(0.11.5)
AM_GNU_GETTEXT
if test "${nls_cv_force_use_gnu_gettext}" = "yes"; then
  AC_DEFINE(HAVE_INCLUDED_GETTEXT, 1, Define if we use the local libintl)
  INCLUDES="${INCLUDES} -I\$(top_builddir)/intl"
else
  AC_CHECK_FUNCS(textdomain,,[
    AC_CHECK_LIB(intl,textdomain,
      AX_ADD_LDFLAGS([vlc],[${LIBINTL}]),,
      ${LIBINTL}
    )
  ])
fi
AM_CONDITIONAL(BUILD_INTL, test "${nls_cv_force_use_gnu_gettext}" = "yes")
XGETTEXT="${XGETTEXT} --keyword=_NS --keyword=_ANS"

AC_MSG_CHECKING(for suffix of libraries)
AC_MSG_RESULT(${LIBEXT})

dnl Check for the need to include the mingwex lib for mingw32
if test "${SYS}" = "mingw32"
then
    AC_CHECK_LIB(mingwex,opendir,
        AC_CHECK_LIB(mingw32,opendir,AX_ADD_LDFLAGS([vlc],[]),
            [AX_ADD_LDFLAGS([vlc gtk],[-lmingwex])])
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
if test "${SYS}" = "mingw32"
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

dnl Flags for plugin compilation
case "${SYS}" in
  mingw32|cygwin)
    AX_ADD_CFLAGS([pic plugin mozilla],[${CFLAGS_mingw32_special}])
    AX_ADD_CXXFLAGS([pic plugin mozilla],[${CFLAGS_mingw32_special} ${CXXFLAGS_mingw32_special}])
    AX_ADD_OBJCFLAGS([pic plugin mozilla],[${CFLAGS_mingw32_special}])
    ;;
  *)
    AX_ADD_CFLAGS([pic plugin mozilla],[-fpic -fPIC])
    AX_ADD_CXXFLAGS([pic plugin mozilla],[-fpic -fPIC])
    AX_ADD_OBJCFLAGS([pic plugin mozilla],[-fpic -fPIC])
    AX_ADD_LDFLAGS([plugin mozilla],[-fpic -fPIC])
    ;;
esac

dnl The -DSYS_FOO flag
CPPFLAGS_save="${CPPFLAGS_save} -DSYS_`echo ${SYS} | sed -e 's/-.*//' | tr 'abcdefghijklmnopqrstuvwxyz.' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ_'`"; CPPFLAGS="${CPPFLAGS_save}"

dnl Check for system libs needed
need_libc=false

AC_CHECK_FUNCS(gettimeofday select strerror strtod strtol isatty vasprintf swab sigrelse getpwuid memalign posix_memalign gethostbyname2 if_nametoindex atoll getenv putenv setenv gmtime_r ctime_r)

dnl Check for usual libc functions
AC_CHECK_FUNCS(strdup strndup atof lseek)
AC_CHECK_FUNCS(strcasecmp,,[AC_CHECK_FUNCS(stricmp)])
AC_CHECK_FUNCS(strncasecmp,,[AC_CHECK_FUNCS(strnicmp)])

AC_CHECK_FUNCS(connect,,[
  AC_CHECK_LIB(socket,connect,[
    AX_ADD_LDFLAGS([vlc ipv4],-lsocket)
  ])
])

AC_CHECK_FUNCS(send,,[
  AC_CHECK_LIB(socket,send,[
    AX_ADD_LDFLAGS([access_http access_mms access_udp access_ftp sap http access_output_udp],[-lsocket])
  ])
])

AC_CHECK_FUNCS(gethostbyname,,[
  AC_CHECK_LIB(nsl,gethostbyname,[
    AX_ADD_LDFLAGS([ipv4 httpd vlc],[-lnsl])
  ],[
    AC_CHECK_LIB(bind,gethostbyname,[
      AX_ADD_LDFLAGS([ipv4 httpd],[-lbind])
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
    AX_ADD_LDFLAGS([ipv4 httpd],[-lresolv])
  ])
])

dnl Check for getopt (always use builtin one on win32)
if test "${SYS}" = "mingw32"; then
need_getopt=:
else
need_getopt=false
AC_CHECK_FUNCS(getopt_long,[AC_DEFINE(HAVE_GETOPT_LONG,1,long getopt support)],
[ # FreeBSD has a gnugetopt library for this:
  AC_CHECK_LIB([gnugetopt],[getopt_long],
    [AC_DEFINE(HAVE_GETOPT_LONG,1,getopt support)
     AX_ADD_LDFLAGS([vlc],[-lgnugetopt])],
    [need_getopt=:])])
fi
AM_CONDITIONAL(BUILD_GETOPT, ${need_getopt})

if test "${SYS}" != "mingw32"; then
AC_TYPE_SIGNAL
AC_CHECK_LIB(dl,dlopen,[
  AX_ADD_LDFLAGS([vlc],[-ldl])
])
AC_CHECK_LIB(m,cos,[
  AX_ADD_LDFLAGS([imdct adjust distort a52tofloat32],[-lm])
])
AC_CHECK_LIB(m,pow,[
  AX_ADD_LDFLAGS([ffmpeg stream_out_transcode imdct imdct3dn imdctsse i420_rgb faad],[-lm])
])
AC_CHECK_LIB(m,sqrt,[
  AX_ADD_LDFLAGS([headphone_channel_mixer],[-lm])
])
fi # end "${SYS}" != "mingw32"

if test "${SYS}" != "mingw32"; then
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

AX_ADD_LDFLAGS([vlc plugin],[${THREAD_LIB}])

dnl Don't link with rt when using GNU-pth
if test "${THREAD_LIB}" != "-lpth" && test "${THREAD_LIB}" != "-lst"; then
  dnl HP/UX port
  AC_CHECK_LIB(rt,sem_init, [AX_ADD_LDFLAGS([vlc],[-lrt])])

  have_nanosleep=false
  AC_CHECK_FUNCS(nanosleep,have_nanosleep=:,[
    AC_CHECK_LIB(rt,nanosleep,
      [AX_ADD_LDFLAGS([vlc],[-lrt]) have_nanosleep=:],
      [AC_CHECK_LIB(posix4,nanosleep,
          [AX_ADD_LDFLAGS([vlc],[-lposix4]) have_nanosleep=:])]
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
fi # end "${SYS}" != "mingw32"

AC_MSG_CHECKING(for strncasecmp in strings.h)
AC_EGREP_HEADER(strncasecmp,strings.h,[
  AC_MSG_RESULT(yes)
  AC_DEFINE(STRNCASECMP_IN_STRINGS_H, 1,
            Define if <strings.h> defines strncasecmp.)],[
  AC_MSG_RESULT(no)])

dnl Check for headers
AC_CHECK_HEADERS(signal.h time.h errno.h stdint.h getopt.h strings.h inttypes.h sys/int_types.h wchar.h)
AC_CHECK_HEADERS(sys/sockio.h fcntl.h sys/types.h sys/time.h sys/times.h sys/ioctl.h sys/stat.h)
AC_CHECK_HEADERS(dlfcn.h image.h)
AC_CHECK_HEADERS(arpa/inet.h net/if.h netinet/in.h sys/socket.h)
if test "${SYS}" != "mingw32"; then
AC_CHECK_HEADERS(machine/param.h sys/shm.h)
AC_CHECK_HEADERS(linux/version.h)
fi # end "${SYS}" != "mingw32"

AC_HEADER_TIME

dnl Check for dirent
need_dirent=false
AC_CHECK_HEADERS(dirent.h,,[need_dirent=:])
AM_CONDITIONAL(BUILD_DIRENT, ${need_dirent})

dnl Mac OS X and other OSes don't have declaration for nanosleep
if test "${SYS}" != "mingw32"; then
AC_MSG_CHECKING(for nanosleep in time.h)
AC_EGREP_HEADER(nanosleep,time.h,[
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_DECL_NANOSLEEP, 1,
            Define if <time.h> defines nanosleep.)
],[
  AC_MSG_RESULT(no)
])
fi # end "${SYS}" != "mingw32"

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
if test "${SYS}" != "mingw32"; then
AC_CHECK_HEADERS(cthreads.h pthread.h kernel/scheduler.h kernel/OS.h)
fi # end "${SYS}" != "mingw32"

dnl Default X headers and libraries
if test "${x_includes}" = "NONE"; then
  x_includes="/usr/X11R6/include"
fi
if test "${x_libraries}" = "NONE"; then
  x_libraries="/usr/X11R6/lib"
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

dnl Check for inline function size limit
AC_CACHE_CHECK([if \$CC accepts -finline-limit],
    [ac_cv_c_inline_limit],
    [CFLAGS="${CFLAGS_save} -finline-limit-30000"
     AC_TRY_COMPILE([],,ac_cv_c_inline_limit=yes, ac_cv_c_inline_limit=no)])
if test "${ac_cv_c_inline_limit}" != "no"; then
    CFLAGS_save="${CFLAGS_save} -finline-limit-30000"; CFLAGS="${CFLAGS_save}"
    CXXFLAGS_save="${CXXFLAGS_save} -finline-limit-30000"; CXXFLAGS="${CXXFLAGS_save}"
    OBJCFLAGS_save="${OBJCFLAGS_save} -finline-limit-30000"; OBJCFLAGS="${OBJCFLAGS_save}"
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
AC_CACHE_CHECK([if \$CC accepts -O3],
    [ac_cv_c_o3],
    [CFLAGS="${CFLAGS_save} -O3"
     AC_TRY_COMPILE([],,ac_cv_c_o3=yes, ac_cv_c_o3=no)])
if test "${ac_cv_c_o3}" != "no"; then
    CFLAGS_OPTIM="${CFLAGS_OPTIM} -O3"
fi

AC_CACHE_CHECK([if \$CC accepts -O2],
    [ac_cv_c_o2],
    [CFLAGS="${CFLAGS_save} -O2"
     AC_TRY_COMPILE([],,ac_cv_c_o2=yes, ac_cv_c_o2=no)])
if test "${ac_cv_c_o2}" != "no"; then
    if test "${ac_cv_c_o3}" = "no"; then
        CFLAGS_OPTIM="${CFLAGS_OPTIM} -O2"
    fi
    CFLAGS_NOOPTIM="${CFLAGS_NOOPTIM} -O2"
else
    AC_CACHE_CHECK([if \$CC accepts -O],
        [ac_cv_c_o],
        [CFLAGS="${CFLAGS_save} -O"
         AC_TRY_COMPILE([],,ac_cv_c_o=yes, ac_cv_c_o=no)])
    if test "${ac_cv_c_o}" != "no"; then
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
    AX_ADD_CFLAGS([i420_yuy2_mmx],[-fomit-frame-pointer])
fi

dnl Check for -mdynamic-no-pic
AC_CACHE_CHECK([if \$CC accepts -mdynamic-no-pic],
    [ac_cv_c_dynamic_no_pic],
    [CFLAGS="${CFLAGS_save} -mdynamic-no-pic"
     AC_TRY_COMPILE([],,ac_cv_c_dynamic_no_pic=yes, ac_cv_c_dynamic_no_pic=no)])
if test "${ac_cv_c_dynamic_no_pic}" != "no"; then
    AX_ADD_CFLAGS([builtin],[-mdynamic-no-pic])
    AX_ADD_CFLAGS([libvlc],[-mdynamic-no-pic])
fi

dnl Check for Darwin plugin linking flags
AC_CACHE_CHECK([if \$CC accepts -bundle -undefined error -lcc_dynamic],
    [ac_cv_ld_darwin],
    [CFLAGS="${CFLAGS_save} -bundle -undefined error -lcc_dynamic"
     AC_TRY_COMPILE([],,ac_cv_ld_darwin=yes, ac_cv_ld_darwin=no)])
if test "${ac_cv_ld_darwin}" != "no"; then
    AX_ADD_LDFLAGS([plugin],[-bundle -undefined error -lcc_dynamic])
fi

dnl Check for standard plugin linking flags
dnl BeOS' gcc needs -nostart instead of -shared, even if -shared isn't harmful (just a warning)
if test "${SYS}" = "beos"; then
  AX_ADD_LDFLAGS([plugin mozilla],[-nostart])
else
  AC_CACHE_CHECK([if \$CC accepts -shared],
      [ac_cv_ld_plugins],
      [CFLAGS="${CFLAGS_save} -shared"
       AC_TRY_COMPILE([],, ac_cv_ld_plugins=yes, ac_cv_ld_plugins=no)])
  if test "${ac_cv_ld_plugins}" != "no"; then
    AX_ADD_LDFLAGS([plugin mozilla],[-shared])
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

case "${ARCH}" in
  hppa64)
    AX_ADD_CFLAGS([mpeg_video_old],[-ffunction-sections])
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
#AX_ADD_BUILTINS([mpeg_video_old idct idctclassic motion])
AX_ADD_PLUGINS([dummy rc logger gestures memcpy])
AX_ADD_PLUGINS([es audio m4v mpeg_system ps ts avi asf aac mp4 rawdv])
AX_ADD_PLUGINS([mpeg_audio lpcm a52 dts cinepak])
AX_ADD_PLUGINS([deinterlace invert adjust wall transform distort clone crop motionblur])
AX_ADD_PLUGINS([float32tos16 float32tos8 float32tou16 float32tou8 a52tospdif dtstospdif fixed32tofloat32 fixed32tos16 s16tofixed32 s16tofloat32 s16tofloat32swab s8tofloat32 u8tofixed32 u8tofloat32])
AX_ADD_PLUGINS([trivial_resampler ugly_resampler linear_resampler bandlimited_resampler])
AX_ADD_PLUGINS([trivial_channel_mixer headphone_channel_mixer])
AX_ADD_PLUGINS([trivial_mixer spdif_mixer float32_mixer])
AX_ADD_PLUGINS([aout_file])
#AX_ADD_PLUGINS([scope])
AX_ADD_PLUGINS([i420_rgb i420_yuy2 i422_yuy2 i420_ymga])
AX_ADD_PLUGINS([id3 m3u])
AX_ADD_PLUGINS([rawvideo])
AX_ADD_PLUGINS([wav araw demuxdump demuxsub adpcm a52sys au])
AX_ADD_PLUGINS([access_file access_udp access_http ipv4 access_mms])
AX_ADD_PLUGINS([access_ftp access_directory sap httpd http])

dnl
dnl  Switch to enable a version of VLC where most modules will be builtin
dnl
AC_ARG_ENABLE(mostly-builtin,
[  --enable-mostly-builtin most modules will be built-in (default disabled)])
if test "${enable_mostly_builtin}" = "yes"; then
  AX_ADD_BUILTINS([${PLUGINS}])
  PLUGINS=""
fi

dnl
dnl Some plugins aren't useful on some platforms
dnl
if test "${SYS}" != "mingw32"; then
    AX_ADD_PLUGINS([screensaver])
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
ALTIVEC_MODULES="memcpyaltivec"
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
  ACCEL_MODULES="${ACCEL_MODULES} ${MMX_MODULES}"
fi

AC_CACHE_CHECK([if \$CC groks MMX EXT inline assembly],
    [ac_cv_mmxext_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("maskmovq %%mm1,%%mm2"::"r"(p));],
                    ac_cv_mmxext_inline=yes, ac_cv_mmxext_inline=no)])
if test "${ac_cv_mmxext_inline}" != "no"; then
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
if test "${ac_cv_sse_inline}" != "no" -a "${SYS}" != "mingw32"; then
  AC_DEFINE(CAN_COMPILE_SSE, 1, Define if \$CC groks SSE inline assembly.)
  ACCEL_MODULES="${ACCEL_MODULES} ${SSE_MODULES}"
fi

if test "${SYS}" != "mingw32"; then
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
    AX_ADD_CFLAGS([idctaltivec],[${ac_cv_altivec_inline}])
    AX_ADD_CFLAGS([motionaltivec],[${ac_cv_altivec_inline}])
    AX_ADD_CFLAGS([memcpyaltivec],[${ac_cv_altivec_inline}])
    AX_ADD_CFLAGS([vlc],[${ac_cv_altivec_inline}])
  fi
  ACCEL_MODULES="${ACCEL_MODULES} ${ALTIVEC_MODULES}"
fi

AC_CACHE_CHECK([if \$CC groks AltiVec C extensions],
    [ac_cv_c_altivec],
    [CFLAGS="${CFLAGS_save} -faltivec"
     # Darwin test
     AC_TRY_COMPILE(,[vec_mtvscr((vector unsigned int)(0));],
        ac_cv_c_altivec=-faltivec, [
        # Linux/PPC test
        CFLAGS="${CFLAGS_save} ${CFLAGS_idctaltivec} -fvec"
        AC_TRY_COMPILE(,[vec_mtvscr((vector unsigned int)(0));],
            [ac_cv_c_altivec="-fvec"], ac_cv_c_altivec=no)
        ])
     CFLAGS="${CFLAGS_save}"])
if test "${ac_cv_c_altivec}" != "no"; then
  AC_DEFINE(CAN_COMPILE_C_ALTIVEC, 1, Define if your compiler groks C AltiVec extensions.)
  AX_ADD_CFLAGS([vlc idctaltivec motionaltivec memcpyaltivec],[${ac_cv_c_altivec}])
  ACCEL_MODULES="${ACCEL_MODULES} ${ALTIVEC_MODULES}"
fi

AC_CACHE_CHECK([if linker needs -framework vecLib],
    [ac_cv_ld_altivec],
    [LDFLAGS="${LDFLAGS_vlc} -framework vecLib"
     AC_TRY_LINK([],,ac_cv_ld_altivec=yes,ac_cv_ld_altivec=no)
     LDFLAGS="${LDFLAGS_save}"
    ])
if test "${ac_cv_ld_altivec}" != "no"; then
  AX_ADD_LDFLAGS([vlc idctaltivec motionaltivec memcpyaltivec],[-framework vecLib])
fi
fi # end "${SYS}" != "mingw32"

AC_ARG_WITH(,[])
AC_ARG_WITH(,[Optimization options:])

dnl
dnl  Special arch tuning
dnl
AC_ARG_WITH(tuning,
[  --with-tuning=ARCH      enable special tuning for an architecture
                          (default i686 on IA-32 and 750 on PPC)])
if test -n "${with_tuning}"; then
    if test "${target_cpu}" = "powerpc"; then
        CFLAGS_TUNING="-mtune=${with_tuning}"
    else
        CFLAGS_TUNING="-mcpu=${with_tuning}"
    fi
else
    if test "${target_cpu}" = "i686" -o "${target_cpu}" = "i586" -o "${target_cpu}" = "i486" -o "${target_cpu}" = "i386"; then CFLAGS_TUNING="-mcpu=pentiumpro"
    else
        if test "${target_cpu}" = "powerpc"; then CFLAGS_TUNING="-mtune=750"; fi
    fi
fi

dnl
dnl  x86 accelerations
dnl
if test "${target_cpu}" = "i686" -o "${target_cpu}" = "i586" -o "${target_cpu}" = "x86" -o "${target_cpu}" = "i386"
then
    ARCH="${ARCH} mmx"
    AX_ADD_BUILTINS([${ACCEL_MODULES}])
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
    AX_ADD_BUILTINS([${ACCEL_MODULES}]) fi ],
[ if test "${target_cpu}" = "powerpc"; then ARCH="${ARCH} altivec";
    AX_ADD_BUILTINS([${ACCEL_MODULES}]) fi ])

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
  AX_ADD_PLUGINS([access_output_dummy access_output_udp access_output_file access_output_http])
  AX_ADD_PLUGINS([mux_ts mux_ps mux_avi mux_mp4 mux_dummy])
  AX_ADD_PLUGINS([packetizer_mpegaudio packetizer_mpegvideo packetizer_a52])
  AX_ADD_PLUGINS([packetizer_mpeg4video packetizer_mpeg4audio])
  AX_ADD_PLUGINS([packetizer_copy])

  AX_ADD_PLUGINS([stream_out_dummy stream_out_standard stream_out_es])
  AX_ADD_PLUGINS([stream_out_duplicate stream_out_display])

  dnl Ogg and vorbis are handled in their respective section
fi


dnl
dnl  Input plugins
dnl

AC_ARG_WITH(,[Input plugins:])

dnl
dnl  DVD module: optionally check for installed libdvdcss
dnl
AC_ARG_ENABLE(dvd,
[  --enable-dvd            DVD input module (default enabled)])
if test "${enable_dvd}" != "no"
then
  AC_ARG_WITH(dvdcss,
  [    --with-dvdcss=PATH    libdvdcss headers and libraries])
  AC_ARG_WITH(dvdcss-tree,
  [    --with-dvdcss-tree=PATH libdvdcss tree for static linking])
  case "${with_dvdcss}" in
  ""|yes)
    if test -z "${with_dvdcss_tree}"
    then
      AC_CHECK_HEADERS(dvdcss/dvdcss.h,
        [ AX_ADD_PLUGINS([dvd])
          AX_ADD_LDFLAGS([dvd dvdcss],[-ldvdcss])],
        [ AC_MSG_WARN([libdvdcss is no longer provided with vlc; please get libdvdcss from http://www.videolan.org/libdvdcss/ and build it. Then either use --with-dvdcss=<path/where/libdvdcss/was/installed> for dynamic linking (recommended under Unix) or --with-dvdcss-tree=<path/where/libdvdcss/was/built> for static linking (recommended under BeOS, Windows, MacOS X). Alternatively you can use --disable-dvd to disable the DVD plugin.])
          AC_MSG_ERROR([cannot find libdvdcss headers]) ])
    else
      AC_MSG_CHECKING(for libdvdcss.a in ${with_dvdcss_tree})
      real_dvdcss_tree="`cd ${with_dvdcss_tree} 2>/dev/null && pwd`"
      if test -z "${real_dvdcss_tree}"
      then
        dnl  The given directory can't be found
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot cd to ${with_dvdcss_tree}])
      fi
      if test -f "${real_dvdcss_tree}/src/.libs/libdvdcss.a"
      then
        dnl  Use a custom libdvdcss
        AC_MSG_RESULT(${real_dvdcss_tree}/src/.libs/libdvdcss.a)
        AX_ADD_BUILTINS([dvd])
        AX_ADD_LDFLAGS([dvd dvdcss],[-L${real_dvdcss_tree}/src/.libs -ldvdcss])
        AX_ADD_CPPFLAGS([dvd],[-I${real_dvdcss_tree}/src])
      else
        dnl  The given libdvdcss wasn't built
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot find ${real_dvdcss_tree}/src/.libs/libdvdcss.a, make sure you compiled libdvdcss in ${with_dvdcss_tree}])
      fi
    fi
  ;;
  no)
    dnl  Compile without dvdcss (dlopen version, works only under Linux)
    AX_ADD_PLUGINS([dvd])
    AX_ADD_CPPFLAGS([dvd],[-DGOD_DAMN_DMCA])
    AX_ADD_LDFLAGS([dvd],[-ldl])
  ;;
  *)
    AC_MSG_CHECKING(for dvdcss headers in ${with_dvdcss})
    if test -f ${with_dvdcss}/include/dvdcss/dvdcss.h
    then
      dnl  Use ${with_dvdcss}/include/dvdcss/dvdcss.h
      AC_MSG_RESULT(yes)
      AX_ADD_PLUGINS([dvd])
      AX_ADD_LDFLAGS([dvd dvdcss],[-L${with_dvdcss}/lib -ldvdcss])
      AX_ADD_CPPFLAGS([dvd],[-I${with_dvdcss}/include])
    else
      dnl  No libdvdcss could be found, sorry
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${with_dvdcss}/include/dvdcss/dvdcss.h])
    fi
  ;;
  esac
fi

dnl
dnl dvdread module: check for libdvdread
dnl
AC_ARG_ENABLE(dvdread,
[  --enable-dvdread        dvdread input module (default disabled)])
if test "${enable_dvdread}" != "no"
then
  AC_ARG_WITH(dvdread,
  [    --with-dvdread=PATH    libdvdread headers and libraries])
  AC_ARG_WITH(dvdread-tree,
  [    --with-dvdread-tree=PATH libdvdread tree for static linking])
  if test -z "${with_dvdread}"
  then
    if test -z "${with_dvdread_tree}"
    then
      AC_CHECK_HEADERS(dvdread/dvd_reader.h,
        [ AX_ADD_PLUGINS([dvdread])
          AX_ADD_LDFLAGS([dvdread],[-ldvdread ${LDFLAGS_dvdcss}])
        ],[
          if test -n "${enable_dvdread}"
          then
            AC_MSG_WARN([Please get libdvdread from http://www.dtek.chalmers.se/groups/dvd/downloads.shtml])
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
        AX_ADD_BUILTINS([dvdread])
        AX_ADD_LDFLAGS([dvdread],[-L${real_dvdread_tree}/dvdread/.libs -ldvdread ${LDFLAGS_dvdcss}])
        AX_ADD_CPPFLAGS([dvdread],[-I${real_dvdread_tree}])
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
      AX_ADD_PLUGINS([dvdread])
      AX_ADD_LDFLAGS([dvdread],[-L${with_dvdread}/lib -ldvdread ${LDFLAGS_dvdcss}])
      AX_ADD_CPPFLAGS([dvdread],[-I${with_dvdread}/include])
    else
      dnl  No libdvdread could be found, sorry
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${with_dvdread}/include/dvdread/dvd_reader.h])
    fi
  fi
fi

dnl
dnl dvdplay module: check for libdvdplay
dnl
AC_ARG_ENABLE(dvdplay,
[  --enable-dvdplay        dvdplay input module (default enabled)])
if test "${enable_dvdplay}" != "no"
then
  AC_ARG_WITH(dvdplay,
  [    --with-dvdplay=PATH    libdvdplay headers and libraries])
  AC_ARG_WITH(dvdplay-tree,
  [    --with-dvdplay-tree=PATH libdvdplay tree for static linking])
  if test -z "${with_dvdplay}"
  then
    if test -z "${with_dvdplay_tree}"
    then
      AC_CHECK_HEADERS(dvdplay/dvdplay.h,[
          AX_ADD_PLUGINS([dvdplay])
          AX_ADD_LDFLAGS([dvdplay],[-ldvdplay ${LDFLAGS_dvdread} ${LDFLAGS_dvdcss}])
          AX_ADD_CPPFLAGS([dvdplay],[${CPPFLAGS_dvdread}])
        ],[
          if test -n "${enable_dvdplay}"
          then
            AC_MSG_WARN([Please get libdvdplay from http://www.videolan.org/.])
            AC_MSG_ERROR([cannot find libdvdplay headers])
          fi
        ])
    else
      AC_MSG_CHECKING(for libdvdplay.a in ${with_dvdplay_tree})
      real_dvdplay_tree="`cd ${with_dvdplay_tree} 2>/dev/null && pwd`"
      if test -z "${real_dvdplay_tree}"
      then
        dnl  The given directory can't be found
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot cd to ${with_dvdplay_tree}])
      fi
      if test -f "${real_dvdplay_tree}/src/.libs/libdvdplay.a"
      then
        dnl  Use a custom libdvdplay
        AC_MSG_RESULT(${real_dvdplay_tree}/src/.libs/libdvdplay.a)
        AX_ADD_BUILTINS([dvdplay])
        AX_ADD_LDFLAGS([dvdplay],[-L${real_dvdplay_tree}/src/.libs -ldvdplay ${LDFLAGS_dvdread} ${LDFLAGS_dvdcss}])
        AX_ADD_CPPFLAGS([dvdplay],[${CPPFLAGS_dvdread} -I${real_dvdplay_tree}/src])
      else
        dnl  The given libdvdplay wasn't built
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot find ${real_dvdplay_tree}/src/.libs/libdvdplay.a, make sure you compiled libdvdplay in ${with_dvdplay_tree}])
      fi
    fi
  else
    AC_MSG_CHECKING(for dvdplay headers in ${with_dvdplay})
    if test -f "${with_dvdplay}/include/dvdplay/dvdplay.h"
    then
      dnl  Use ${with_dvdplay}/include/dvdplay/dvdplay.h
      AC_MSG_RESULT(yes)
      AX_ADD_PLUGINS([dvdplay])
      AX_ADD_LDFLAGS([dvdplay],[-L${with_dvdplay}/lib -ldvdplay ${LDFLAGS_dvdread} ${LDFLAGS_dvdcss}])
      AX_ADD_CPPFLAGS([dvdplay],[${CPPFLAGS_dvdread} -I${with_dvdplay}/include])
    else
      dnl  No libdvdplay could be found, sorry
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${with_dvdplay}/include/dvdplay/dvdplay.h])
    fi
  fi
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
        [ AX_ADD_PLUGINS([ts_dvbpsi mux_ts_dvbpsi])
          AX_ADD_LDFLAGS([ts_dvbpsi mux_ts_dvbpsi],[-ldvbpsi]) ],
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
        AX_ADD_BUILTINS([ts_dvbpsi mux_ts_dvbpsi])
        AX_ADD_CPPFLAGS([ts_dvbpsi mux_ts_dvbpsi],[-I${real_dvbpsi_tree}/src])
        AX_ADD_LDFLAGS([ts_dvbpsi mux_ts_dvbpsi],[-L${real_dvbpsi_tree}/src/.libs -ldvbpsi])
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
      AX_ADD_PLUGINS([ts_dvbpsi mux_ts_dvbpsi])
      AX_ADD_CPPFLAGS([ts_dvbpsi mux_ts_dvbpsi],[${CPPFLAGS_test}])
      AX_ADD_LDFLAGS([ts_dvbpsi mux_ts_dvbpsi],[${LDFLAGS_test} -ldvbpsi])

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
    AX_ADD_CPPFLAGS([v4l],[-I${with_v4l}/include])
  fi

  CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_v4l}"
  AC_CHECK_HEADERS(linux/videodev.h, [
    AX_ADD_PLUGINS([v4l])
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
  AX_ADD_PLUGINS([pvr])
fi

dnl
dnl  VCD module
dnl
AC_ARG_ENABLE(vcd,
  [  --enable-vcd            VCD support for Linux, FreeBSD, MacOS X and Win32 (default enabled)])

if test "${enable_vcd}" != "no"
then
  AC_MSG_CHECKING(for cdrom_msf0 in linux/cdrom.h)
  AC_EGREP_HEADER(cdrom_msf0,linux/cdrom.h,[
    AC_MSG_RESULT(yes)
    AX_ADD_PLUGINS([vcd cdda])
  ],[
    AC_MSG_RESULT(no)
  ])

  AC_MSG_CHECKING(for scsireq in sys/scsiio.h)
  AC_EGREP_HEADER(scsireq,sys/scsiio.h,[
    AC_MSG_RESULT(yes)
    AX_ADD_PLUGINS([vcd cdda])
    AC_DEFINE(HAVE_SCSIREQ_IN_SYS_SCSIIO_H, 1, For NetBSD VCD support)
  ],[
    AC_MSG_RESULT(no)
  ])

  AC_MSG_CHECKING(for ioc_toc_header in sys/cdio.h)
  AC_EGREP_HEADER(ioc_toc_header ,sys/cdio.h,[
    AC_MSG_RESULT(yes)
    AX_ADD_PLUGINS([vcd cdda])
    AC_DEFINE(HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H, 1, For FreeBSD VCD support)
  ],[
    AC_MSG_RESULT(no)
  ])

  if test "${SYS}" = "bsdi" -o "${SYS}" = "mingw32"
  then
    AX_ADD_PLUGINS([vcd cdda])
  fi

  if test "${SYS}" = "darwin"
  then
    AX_ADD_PLUGINS([vcd cdda])
    AX_ADD_LDFLAGS([vcd cdda],[-framework IOKit -framework CoreFoundation])
  fi
fi

dnl
dnl  Satellite input module
dnl
AC_ARG_ENABLE(satellite,
  [  --enable-satellite      satellite card support (default disabled)],
  [ if test "${enable_satellite}" = "yes"
    then
      AX_ADD_PLUGINS([satellite])
    fi])

dnl
dnl  ipv6 plugin - not for QNX yet
dnl
if test "${SYS}" != "nto" && test "${SYS}" != "mingw32"
then
  have_ipv6=false
  AC_CHECK_FUNCS(inet_pton,[have_ipv6=:],[
    AC_CHECK_LIB(resolv,inet_pton,
      [have_ipv6=:
       AX_ADD_LDFLAGS([ipv6],[-lresolv])])
  ])
  AC_MSG_CHECKING(for sockaddr_in6 in netinet/in.h)
  AC_EGREP_HEADER(sockaddr_in6,netinet/in.h,
    [AC_MSG_RESULT(yes)], [AC_MSG_RESULT(no) have_ipv6=false])
  if ${have_ipv6}; then
    AX_ADD_PLUGINS([ipv6])
  fi
fi
if test "${SYS}" = "mingw32"
then
  AC_MSG_CHECKING(for getaddrinfo in ws2tcpip.h)
  AC_EGREP_HEADER(addrinfo,ws2tcpip.h,[AC_MSG_RESULT(yes)
    AX_ADD_PLUGINS([ipv6])],[AC_MSG_RESULT(no)])
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
      AC_CHECK_LIB( ogg, oggpackB_read, [
        AX_ADD_CPPFLAGS([ogg],[-DHAVE_OGGPACKB])])
      AX_ADD_PLUGINS([ogg mux_ogg])
      AX_ADD_LDFLAGS([ogg mux_ogg],[-logg])])
   ],[])
fi

dnl
dnl  matroska demux plugin
dnl
AC_ARG_ENABLE(mkv,
  [  --enable-mkv            Matroska demux support (default disabled)])
if test "${enable_mkv}" = "yes"
then
AC_LANG_PUSH(C++)
  dnl matroska headers include ebml headers directly... pouah that stinks
  CPPFLAGS="${CPPFLAGS_save} -I/usr/local/include/ebml -I/usr/include/ebml"
  AC_CHECK_HEADERS(EbmlVersion.h matroska/KaxVersion.h, [
    AX_ADD_PLUGINS([mkv])
    AX_ADD_CXXFLAGS([mkv],[-I/usr/local/include/ebml -I/usr/include/ebml])
    AC_CHECK_LIB(ebml_pic,main,[
      # We have ebml_pic, that's good, we can build an mkv.so plugin !
      AX_ADD_LDFLAGS([mkv],[-lmatroska_pic -lebml_pic])
    ],[
      AX_ADD_LDFLAGS([mkv],[-lmatroska -lebml])
    ])
  ],[
    AC_MSG_ERROR([libebml or libmatroska include files not found])
  ])
  CPPFLAGS="${CPPFLAGS_save}"
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
    AX_ADD_CPPFLAGS([mpgatofixed32],[-I${with_mad}/include])
    AX_ADD_LDFLAGS([mpgatofixed32],[-L${with_mad}/lib])
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
    AC_MSG_CHECKING(for mad.h in ${real_mad_tree}/libmad)
    if test -f ${real_mad_tree}/libmad/mad.h
    then
      AC_MSG_RESULT(yes)
      AX_ADD_CPPFLAGS([mpgatofixed32],[-I${real_mad_tree}/libmad])
      AX_ADD_LDFLAGS([mpgatofixed32],[-L${real_mad_tree}/libmad/.libs])
      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_mpgatofixed32}"
      AC_CHECK_LIB(mad, mad_bit_init, [
        AX_ADD_BUILTINS([mpgatofixed32])
        AX_ADD_LDFLAGS([mpgatofixed32],[-lmad])
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
      AX_ADD_PLUGINS([mpgatofixed32])
      AX_ADD_LDFLAGS([mpgatofixed32],[-lmad])],
      [ AC_MSG_ERROR([Cannot find libmad library...]) ])
    CPPFLAGS="${CPPFLAGS_save}"
    LDFLAGS="${LDFLAGS_save}"
  fi
fi

dnl
dnl   libid3tag support
dnl
AC_CHECK_HEADERS(id3tag.h, [
  AC_CHECK_HEADERS(zlib.h, [
    AX_ADD_LDFLAGS([id3tag],[-lid3tag -lz])
    AX_ADD_PLUGINS([id3tag])]) ])

dnl
dnl  ffmpeg decoder plugin
dnl
AC_ARG_ENABLE(ffmpeg,
[  --enable-ffmpeg         ffmpeg codec (default enabled)])
if test "${enable_ffmpeg}" != "no"
then
  AC_ARG_WITH(ffmpeg-mp3lame,
    [    --with-ffmpeg-mp3lame   if ffmpeg has been compiled with mp3lame support],
    [ AX_ADD_LDFLAGS([stream_out_transcode],[-lmp3lame])
      AX_ADD_LDFLAGS([ffmpeg],[-lmp3lame]) ])

  AC_ARG_WITH(ffmpeg-tree,
    [    --with-ffmpeg-tree=PATH ffmpeg tree for static linking])

  dnl
  dnl test for !(--with-ffmpeg-tree)
  dnl
  if test "${with_ffmpeg_tree}" = "no" -o -z "${with_ffmpeg_tree}";then
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_ffmpeg}"
    LDFLAGS="${LDFLAGS_save} ${LDFLAGS_ffmpeg}"
    AC_CHECK_HEADERS(ffmpeg/avcodec.h postproc/postprocess.h)
    AC_CHECK_LIB(avcodec, avcodec_init, [
      AX_ADD_BUILTINS([ffmpeg stream_out_transcode])
      AX_ADD_LDFLAGS([ffmpeg],[-lavcodec])
      dnl  XXX: we don't link with -lavcodec a 2nd time because the OS X
      dnl       linker would miserably barf on multiple definitions.
      AX_ADD_LDFLAGS([stream_out_transcode],[]) ],
       [ AC_MSG_ERROR([Could not find ffmpeg on your system: you may get it from http://ffmpeg.sf.net/ (cvs version is recommended). Alternatively you can use --disable-ffmpeg to disable the ffmpeg plugins.]) ])
    LDFLAGS="${LDFLAGS_save}"
    CPPFLAGS="${CPPFLAGS_save}"
  fi

  dnl
  dnl test for --with-ffmpeg-tree
  dnl
  if test "${with_ffmpeg_tree}" != "no" -a -n "${with_ffmpeg_tree}";then
    AC_MSG_CHECKING(for libavcodec.a in ${with_ffmpeg_tree})
    real_ffmpeg_tree="`cd ${with_ffmpeg_tree} 2>/dev/null && pwd`"
    if test -z "${real_ffmpeg_tree}"; then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_ffmpeg_tree}])
    fi
    if test -f "${real_ffmpeg_tree}/libavcodec/libavcodec.a"; then
      dnl  Use a custom libffmpeg
      AC_MSG_RESULT(${real_ffmpeg_tree}/libavcodec/libavcodec.a)
      AX_ADD_BUILTINS([ffmpeg stream_out_transcode])
      AX_ADD_LDFLAGS([ffmpeg],[-L${real_ffmpeg_tree}/libavcodec -lavcodec])
      AX_ADD_CPPFLAGS([ffmpeg],[-I${real_ffmpeg_tree}/libavcodec])

      dnl  XXX: we don't link with -lavcodec a 2nd time because the OS X
      dnl       linker would miserably barf on multiple definitions.
      AX_ADD_LDFLAGS([stream_out_transcode],[-L${real_ffmpeg_tree}/libavcodec])
      AX_ADD_CPPFLAGS([stream_out_transcode],[-I${real_ffmpeg_tree}/libavcodec])
    else
      dnl  The given libavcodec wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_ffmpeg_tree}/libavcodec/libavcodec.a, make sure you compiled libavcodec in ${with_ffmpeg_tree}])
    fi
  fi

  dnl Duplicate the ffmpeg CPPFLAGS and LDFLAGS for the encoder
  AX_ADD_CPPFLAGS([encoder_ffmpeg],[${CPPFLAGS_ffmpeg}])
  dnl XXX: we don't link with -lavcodec blah blah blah
  dnl AX_ADD_LDFLAGS([encoder_ffmpeg],[${LDFLAGS_ffmpeg}])

  ac_have_vorbis_headers=yes
  AC_CHECK_HEADERS(vorbis/vorbisenc.h vorbis/codec.h,,
      ac_have_vorbis_headers=no)
  if test "$ac_have_vorbis_headers" = "yes"; then
    AX_ADD_LDFLAGS([stream_out_transcode],[-lvorbisenc -lvorbis -logg])
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
      AX_ADD_BUILTINS([faad])
      AX_ADD_LDFLAGS([faad],[-L${real_faad_tree}/libfaad/.libs -lfaad])
      AX_ADD_CPPFLAGS([faad],[-I${real_faad_tree}/include])
      CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_faad}"
      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_faad}"
      AC_TRY_COMPILE([#include <faad.h>],
        [faacDecHandle a; faacDecFrameInfo *b; unsigned char *c;
         unsigned long d; faacDecDecode(a,b,c,d);],
        [], [AC_DEFINE(HAVE_OLD_FAAD2, 1, Define if FAAD2 < 1.2)])
      LDFLAGS="${LDFLAGS_save}"
      CPPFLAGS="${CPPFLAGS_save}"
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
      AX_ADD_PLUGINS([faad])
      AX_ADD_LDFLAGS([faad],[-lfaad])
      AC_TRY_COMPILE([#include <faad.h>],
        [faacDecHandle a; faacDecFrameInfo *b; unsigned char *c;
         unsigned long d; faacDecDecode(a,b,c,d);],
        [], [AC_DEFINE(HAVE_OLD_FAAD2, 1, Define if FAAD2 < 1.2)]) ],
      [ AC_MSG_ERROR([Cannot find libfaad library...]) ])
    LDFLAGS="${LDFLAGS_save}"
    CPPFLAGS="${CPPFLAGS_save}"
  fi
fi


dnl
dnl  xvid decoder plugin
dnl
AC_ARG_ENABLE(xvid,
[  --enable-xvid           xvid codec (default disabled)])
if test "${enable_xvid}" = "yes"
then
  AC_ARG_WITH(xvid,
    [    --with-xvid=PATH      path to xvid installation],[],[])
  if test "${with_xvid}" != "no" -a -n "${with_xvid}"
  then
    AX_ADD_CPPFLAGS([xvid],[-I${with_xvid}/include])
    AX_ADD_LDFLAGS([xvid],[-L${with_xvid}/lib])
  fi

  AC_ARG_WITH(xvid-tree,
  [    --with-xvid-tree=PATH xvid tree for static linking])
  if test -n "${with_xvid_tree}"
  then
    AC_MSG_CHECKING(for libxvidcore.a in ${with_xvid_tree})
    real_xvid_tree="`cd ${with_xvid_tree} 2>/dev/null && pwd`"
    if test -z "${real_xvid_tree}"
    then
      dnl  The given directory can't be found
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot cd to ${with_xvid_tree}])
    fi
    if test -f "${real_xvid_tree}/build/generic/libxvidcore.a"
    then
      dnl  Use a custom xvid
      AC_MSG_RESULT(${real_xvid_tree}/build/generic/libxvidcore.a)
      AX_ADD_BUILTINS([xvid])
      AX_ADD_LDFLAGS([xvid],[-L${real_xvid_tree}/build/generic -lxvidcore])
      AX_ADD_CPPFLAGS([xvid],[-I${real_xvid_tree}/src])
    else
      dnl  The given libxvidcore wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_xvid_tree}/build/generic/libxvidcore.a, make sure you compiled libxvidcore in ${with_xvid_tree}])
    fi
  else
    CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_xvid}"
    LDFLAGS="${LDFLAGS_save} ${LDFLAGS_xvid}"
    AC_CHECK_HEADERS(xvid.h, ,
      [ AC_MSG_ERROR([Cannot find development headers for libxvidcore...]) ])
    AC_CHECK_LIB(xvidcore, xvid_init, [
      AX_ADD_PLUGINS([xvid])
      AX_ADD_LDFLAGS([xvid],[-lxvidcore]) ],
      [ AC_MSG_ERROR([Cannot find libxvidcore library...]) ])
    LDFLAGS="${LDFLAGS_save}"
    CPPFLAGS="${CPPFLAGS_save}"
  fi

  dnl Duplicate the xvid CPPFLAGS and LDFLAGS for the encoder
  AX_ADD_LDFLAGS([encoder_xvid],[${LDFLAGS_xvid}])
  AX_ADD_CPPFLAGS([encoder_xvid],[${CPPFLAGS_xvid}])
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
    AX_ADD_BUILTINS([quicktime])
  else
  AC_CHECK_HEADERS(QuickTime/QuickTime.h,
    [ AX_ADD_BUILTINS([quicktime])
      AX_ADD_LDFLAGS([quicktime],[-framework QuickTime -framework Carbon])
    ], [ AC_MSG_ERROR([cannot find CoreAudio headers]) ])
  fi
fi

dnl
dnl MP4 module
dnl
AC_CHECK_HEADERS(zlib.h, [
  AX_ADD_LDFLAGS([mp4 skins],[-lz])
] )


dnl
dnl skins module
dnl
AC_CHECK_HEADERS(libtar.h, [
  AX_ADD_LDFLAGS([skins],[-ltar])
] )



dnl
dnl  a52 AC3 decoder plugin
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
      AX_ADD_CPPFLAGS([a52tofloat32],[-I${real_a52_tree}])
      AX_ADD_LDFLAGS([a52tofloat32],[-L${real_a52_tree}/liba52/.libs])
      LDFLAGS="${LDFLAGS_save} ${LDFLAGS_a52tofloat32}"
      AC_CHECK_LIB(a52, a52_free, [
        AX_ADD_BUILTINS([a52tofloat32])
        AX_ADD_CPPFLAGS([a52tofloat32],[-DUSE_A52DEC_TREE])
        AX_ADD_LDFLAGS([a52tofloat32],[-la52])
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
        AX_ADD_PLUGINS([a52tofloat32])
        AX_ADD_LDFLAGS([a52tofloat32],[${LDFLAGS_test} -la52])
        AX_ADD_CPPFLAGS([a52tofloat32],[${CPPFLAGS_test}])
      ],[
        AC_MSG_ERROR([Could not find liba52 on your system: you may get it from http://liba52.sf.net/. Alternatively you can use --disable-a52 to disable the a52 plugin.])
      ])
    ])
    CPPFLAGS="${CPPFLAGS_save}"
    LDFLAGS="${LDFLAGS_save}"
  fi
fi

dnl
dnl  DV plugin
dnl
AC_ARG_ENABLE(dv,
  [  --enable-dv             DV decoder support (deprecated in favor of ffmpeg) (default disabled)])
if test "${enable_dv}" = "yes"
then
  AC_CHECK_HEADERS(libdv/dv.h, [
    AX_ADD_PLUGINS([dv])
    AX_ADD_LDFLAGS([dv],[-ldv])
   ],[])
fi

dnl
dnl  Flac plugin
dnl
AC_ARG_ENABLE(flac,
  [  --enable-flac           flac decoder support (default disabled)])
if test "${enable_flac}" = "yes"
then
  AC_CHECK_HEADERS(FLAC/stream_decoder.h, [
    AX_ADD_PLUGINS([flac flacdec])
    AX_ADD_LDFLAGS([flacdec],[-lFLAC])
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
      AX_ADD_PLUGINS([libmpeg2])
      AX_ADD_LDFLAGS([libmpeg2],[-L${real_libmpeg2_tree}/libmpeg2/.libs -lmpeg2])
      AX_ADD_CFLAGS([libmpeg2],[-I${real_libmpeg2_tree}/include])
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
          AX_ADD_PLUGINS([libmpeg2])
          AX_ADD_LDFLAGS([libmpeg2],[-lmpeg2])],
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
    AX_ADD_PLUGINS([vorbis])
    AX_ADD_LDFLAGS([vorbis],[-lvorbis -logg])
    if test "${enable_sout}" != "no"; then
      AX_ADD_PLUGINS([packetizer_vorbis])
      AX_ADD_LDFLAGS([packetizer_vorbis],[-lvorbis -logg])
    fi ],[])
fi

dnl
dnl  Tremor plugin
dnl
AC_ARG_ENABLE(tremor,
  [  --enable-tremor         Tremor decoder support (default disabled)])
if test "${enable_tremor}" = "yes"
then
  AC_CHECK_HEADERS(tremor/ivorbiscodec.h, [
    AX_ADD_PLUGINS([tremor])
    AX_ADD_LDFLAGS([tremor],[-lvorbisidec -logg])
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
      AX_ADD_BUILTINS([tarkin])
      AX_ADD_CPPFLAGS([tarkin],[-I${real_tarkin_tree}])
      AX_ADD_LDFLAGS([tarkin],[${real_tarkin_tree}/mem.o ${real_tarkin_tree}/pnm.o ${real_tarkin_tree}/wavelet.o ${real_tarkin_tree}/wavelet_xform.o ${real_tarkin_tree}/wavelet_coeff.o ${real_tarkin_tree}/yuv.o ${real_tarkin_tree}/tarkin.o ${real_tarkin_tree}/info.o -logg])
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
      AX_ADD_BUILTINS([theora])
      if test "${SYS}" = "darwin"; then
        theora_libs="-ltheora"
      else
        theora_libs="-ltheora -logg"
      fi
      AX_ADD_LDFLAGS([theora],[${theora_libs}]) ],[
      AC_MSG_ERROR([libtheora doesn't appear to be installed on you system.
You also need to check that you have a libogg posterior to the 1.0 release.])],
      [-logg])
  ])
fi

dnl
dnl   spudec support
dnl
AX_ADD_LDFLAGS([spudec],[${LIBICONV}])
AX_ADD_CPPFLAGS([spudec],[${INCICONV}])
AX_ADD_PLUGINS([spudec])


dnl
dnl  Video Filters
dnl

dnl
dnl   png
dnl
AC_CHECK_HEADERS(png.h, [
    AX_ADD_LDFLAGS([logo],[-lpng])
    AX_ADD_PLUGINS([logo])])

dnl
dnl  Video plugins
dnl

AC_ARG_WITH(,[Video plugins:])

dnl Check for DPMS
if test "${SYS}" != "mingw32"
then
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
  (test "${SYS}" != "mingw32" || test "${enable_x11}" = "yes"); then
  CPPFLAGS="${CPPFLAGS_save} -I${x_includes}"
  AC_CHECK_HEADERS(X11/Xlib.h, [
    AX_ADD_PLUGINS([x11])
    AX_ADD_LDFLAGS([x11],[-L${x_libraries} -lX11 -lXext])
    AX_ADD_CPPFLAGS([x11],[-I${x_includes}])
    AC_CHECK_HEADERS(X11/extensions/Xinerama.h, [
      CFLAGS="${CFLAGS_save} -L${x_libraries} -lX11 -lXext"
      AC_CHECK_LIB(Xinerama, XineramaQueryExtension, [
        AC_DEFINE(HAVE_XINERAMA,1,[Define this if you have libXinerama installed])
        AX_ADD_LDFLAGS([x11],[-lXinerama])
      ])
      CFLAGS="${CFLAGS_save}"
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
  (test "${SYS}" != "mingw32" || test "${enable_xvideo}" = "yes"); then
  CPPFLAGS="${CPPFLAGS_save} -I${x_includes}"
  AC_CHECK_HEADERS(X11/extensions/Xv.h, [
    CFLAGS="${CFLAGS_save} -L${x_libraries} -lX11 -lXext"
    AC_CHECK_LIB(Xv_pic,XvPutImage,
      # We have Xv_pic, that's good, we can build an xvideo.so plugin !
      AX_ADD_PLUGINS([xvideo])
      AX_ADD_LDFLAGS([xvideo],[-L${x_libraries} -lX11 -lXext -lXv_pic])
      AX_ADD_CPPFLAGS([xvideo],[-I${x_includes}]),
      AC_CHECK_LIB(Xv,XvPutImage,[
        # We don't have Xv_pic, but we have Xv, let's make xvideo.a as builtin
        AX_ADD_PLUGINS([xvideo])
        AX_ADD_LDFLAGS([xvideo],[-L${x_libraries} -lX11 -lXext -lXv])
        AX_ADD_CPPFLAGS([xvideo],[-I${x_includes}])
      ],[
        # Otherwise... well, do nothing.
        :
      ])
    )
    AC_CHECK_HEADERS(X11/extensions/Xinerama.h,[
      AC_CHECK_LIB(Xinerama, XineramaQueryExtension,[
        AC_DEFINE(HAVE_XINERAMA,1,[Define this if you have libXinerama installed])
        AX_ADD_LDFLAGS([xvideo],[-lXinerama])
      ])
    ])
    CFLAGS="${CFLAGS_save}"
  ]
  CPPFLAGS="${CPPFLAGS_save}")
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
  if test "${SDL_CONFIG}" = "no"
  then
    AC_PATH_PROG(SDL11_CONFIG, sdl11-config, no, ${SDL_PATH})
    SDL_CONFIG=${SDL11_CONFIG}
    SDL_HEADER="SDL11/SDL.h"
  fi
  if test "${SDL_CONFIG}" = "no"
  then
    AC_PATH_PROG(SDL_CONFIG, sdl-config, no, ${SDL_PATH})
    SDL_HEADER="SDL/SDL.h"
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
    AX_ADD_PLUGINS([vout_sdl aout_sdl])
    AX_ADD_CFLAGS([vout_sdl aout_sdl],[`${SDL_CONFIG} ${SDL_PREFIX} --cflags | sed 's,SDL,,'`])
    AX_ADD_LDFLAGS([vout_sdl aout_sdl],[`${SDL_CONFIG} ${SDL_PREFIX} --libs | sed 's,-rdynamic,,'`])
    CPPFLAGS="${CPPFLAGS_save} ${CFLAGS_vout_sdl}"
    AC_CHECK_HEADERS(${SDL_HEADER}, AC_DEFINE_UNQUOTED(SDL_INCLUDE_FILE,
      <${SDL_HEADER}>, Indicate whether we should use SDL/SDL.h or SDL11/SDL.h),
      [ AC_CHECK_HEADERS(SDL.h, AC_DEFINE(SDL_INCLUDE_FILE, <SDL.h>,
          As a last resort we also test for SDL.h presence),
      [ AC_MSG_ERROR([The development package for SDL is not installed.
Please install it and try again. Alternatively you can also configure with
--disable-sdl.])
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
    AX_ADD_PLUGINS([freetype])
    AX_ADD_CFLAGS([freetype],[`${FREETYPE_CONFIG} --cflags`])
    AX_ADD_LDFLAGS([freetype],[`${FREETYPE_CONFIG} --libs`])
    AC_CHECK_HEADERS(Carbon/Carbon.h,
      [AX_ADD_LDFLAGS([freetype],[-framework Carbon])])
    CPPFLAGS="${CPPFLAGS_save} ${CFLAGS_freetype}"
  elif test "${enable_freetype}" =  "yes"
  then
    AC_MSG_ERROR([I couldn't find the freetype package. You can download libfreetype2
from http://www.freetype.org/, or configure with --disable-freetype. Have a nice day.
    ])
  fi
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
    AX_ADD_LDFLAGS([qte],[-L${with_qte}/lib `echo -L${with_qte}/lib | sed 's,opt/QtPalmtop,usr,'` -ljpeg -lqte])
    AX_ADD_CXXFLAGS([qte],[-I${with_qte}/include `echo -I${with_qte}/include | sed 's,opt/QtPalmtop,usr,'` -DQT_QWS_IPAQ -DQWS -fno-exceptions -fno-rtti])
  else
    AX_ADD_LDFLAGS([qte],[-L${QTDIR}/lib `echo -L${QTDIR}/lib | sed 's,opt/QtPalmtop,usr,'` -ljpeg -lqte])
    AX_ADD_CXXFLAGS([qte],[-I${QTDIR}/include `echo -I${QTDIR}/include | sed 's,opt/QtPalmtop,usr,'` -DQT_QWS_IPAQ -DQWS -fno-exceptions -fno-rtti])
  fi
  AX_ADD_PLUGINS([qte])
  NEED_QTE_MAIN=yes
  CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_qte}"
  AC_CHECK_HEADERS(qt.h jpeglib.h, ,[
    AC_MSG_ERROR([echo "Cannot find QT Embedded development headers."])
  ] )
  CPPFLAGS="${CPPFLAGS_save}"
fi

dnl
dnl  Windows DirectX module
dnl
AC_ARG_ENABLE(directx,
  [  --enable-directx        Win32 DirectX support (default enabled on Win32)])
if test "${enable_directx}" != "no"
then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"
  then
    AC_ARG_WITH(directx,
    [    --with-directx=PATH   Win32 DirectX headers])
    if test -z "${with_directx}"
    then
      AC_CHECK_HEADERS(ddraw.h,
      [ AX_ADD_PLUGINS([vout_directx aout_directx])
        AX_ADD_LDFLAGS([vout_directx],[-lgdi32]) ])
    else
      AC_MSG_CHECKING(for directX headers in ${with_directx})
      if test -f ${with_directx}/ddraw.h
      then
        AX_ADD_PLUGINS([vout_directx aout_directx])
        AX_ADD_CPPFLAGS([vout_directx aout_directx],[-I${with_directx}])
        AX_ADD_LDFLAGS([vout_directx],[-lgdi32])
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
        AX_ADD_PLUGINS([fb])
      ])
    fi

dnl
dnl  Linux MGA module
dnl
AC_ARG_ENABLE(mga,
  [  --enable-mga            Linux kernel Matrox support (default disabled)],
  [ if test "${enable_mga}" = "yes"
    then
      AX_ADD_PLUGINS([mga xmga])
    fi ])

dnl
dnl  SVGAlib module
dnl
AC_ARG_ENABLE(svgalib,
  [  --enable-svgalib        SVGAlib support (default disabled)])
if test "${enable_svgalib}" = "yes"
then
  AX_ADD_PLUGINS([svgalib])
  AX_ADD_LDFLAGS([svgalib],[-lvgagl -lvga])
fi

dnl
dnl  GGI module
dnl
AC_ARG_ENABLE(ggi,
  [  --enable-ggi            GGI support (default disabled)])
if test "${enable_ggi}" = "yes"
then
  AX_ADD_PLUGINS([ggi])
  AX_ADD_LDFLAGS([ggi],[-lggi])
  AC_ARG_WITH(ggi,
    [    --with-ggi=PATH       path to libggi],
    [ if test "${with_ggi}" != "no" -a -n "${with_ggi}"
      then
        AX_ADD_CPPFLAGS([ggi],[-I${with_ggi}/include])
        AX_ADD_LDFLAGS([ggi],[-L${with_ggi}/lib])
      fi ])
fi

dnl
dnl  Glide module
dnl
AC_ARG_ENABLE(glide,
  [  --enable-glide          Glide (3dfx) support (default disabled)])
if test "${enable_glide}" = "yes"
then
  AX_ADD_PLUGINS([glide])
  AX_ADD_LDFLAGS([glide],[-lglide2x -lm])
  AX_ADD_CPPFLAGS([glide],[-I/usr/include/glide])
  AC_ARG_WITH(glide,
    [    --with-glide=PATH     path to libglide],
    [ if test "${with_glide}" != "no" -a -n "${with_glide}"
      then
        AX_ADD_CPPFLAGS([glide],[-I${with_glide}/include])
        AX_ADD_LDFLAGS([glide],[-L${with_glide}/lib])
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
    AX_ADD_PLUGINS([aa])
    AX_ADD_LDFLAGS([aa],[-laa])
  fi
fi

dnl
dnl  win32 GDI plugin
dnl
AC_ARG_ENABLE(wingdi,
  [  --enable-wingdi         Win32 GDI module (default enabled on Win32)])
if test "${enable_wingdi}" != "no"; then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"; then
    AX_ADD_PLUGINS([wingdi])
    AX_ADD_LDFLAGS([wingdi],[-lgdi32])
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
  (test "${SYS}" != "mingw32" || test "${enable_oss}" = "yes")
then
  AC_CHECK_HEADERS(soundcard.h sys/soundcard.h machine/soundcard.h, [
    AX_ADD_PLUGINS([oss])
    AC_CHECK_LIB(ossaudio,main,AX_ADD_LDFLAGS([oss],[-lossaudio]))
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
       AX_ADD_PLUGINS([esd])
       AX_ADD_CFLAGS([esd],[`${ESD_CONFIG} --cflags`])
       AX_ADD_LDFLAGS([esd],[`${ESD_CONFIG} --libs`])
     fi
   fi])

dnl
dnl  aRts module
dnl
AC_ARG_ENABLE(arts,
  [  --enable-arts           aRts sound server (default disabled)],
  [if test "${enable_arts}" = "yes"
   then
     AC_PATH_PROG(ARTS_CONFIG, artsc-config, no)
     if test "${ARTS_CONFIG}" != "no"
     then
       AX_ADD_PLUGINS([arts])
       AX_ADD_CFLAGS([arts],[`${ARTS_CONFIG} --cflags`])
       AX_ADD_LDFLAGS([arts],[`${ARTS_CONFIG} --libs `])
     fi
   fi])

dnl
dnl  ALSA module
dnl
AC_ARG_ENABLE(alsa,
  [  --enable-alsa           ALSA sound support for Linux (default disabled)],
  [if test "${enable_alsa}" = "yes"
   then
     AC_CHECK_HEADER(alsa/asoundlib.h, AC_CHECK_LIB(asound, main, have_alsa="true", have_alsa="false"),have_alsa="false")
     if test "${have_alsa}" = "true"
     then
       AC_TRY_COMPILE([#define ALSA_PCM_NEW_HW_PARAMS_API
                       #define ALSA_PCM_NEW_SW_PARAMS_API
                       #include <alsa/asoundlib.h>],
          [void foo() { snd_pcm_hw_params_get_period_time(0,0,0); }],
           AC_DEFINE(HAVE_ALSA_NEW_API, 1, Define if ALSA is at least rc4))
       AX_ADD_PLUGINS([alsa])
       AX_ADD_LDFLAGS([alsa],[-lasound -lm -ldl])
     else
       AC_MSG_ERROR([Could not find ALSA development headers])
     fi
   fi])

dnl
dnl  win32 waveOut plugin
dnl
AC_ARG_ENABLE(waveout,
  [  --enable-waveout        Win32 waveOut module (default enabled on Win32)])
if test "${enable_waveout}" != "no"; then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"; then
    AX_ADD_PLUGINS([waveout])
    AX_ADD_LDFLAGS([waveout],[-lwinmm])
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
    [ AX_ADD_BUILTINS([coreaudio])
      AX_ADD_LDFLAGS([coreaudio],[-framework CoreAudio])
      AC_MSG_CHECKING(for kAudioConverterPrimeMethod in AudioToolbox/AudioConverter.h)
      AC_EGREP_HEADER(kAudioConverterPrimeMethod,AudioToolbox/AudioConverter.h,[
        AC_MSG_RESULT(yes)
        AX_ADD_PLUGINS([coreaudio_resampler])
        AX_ADD_LDFLAGS([coreaudio_resampler],[-framework AudioToolbox])
      ],[ AC_MSG_RESULT(no) ])
    ], [ AC_MSG_ERROR([cannot find CoreAudio headers]) ])
fi

dnl
dnl  Interface plugins
dnl

AC_ARG_WITH(,[Interface plugins:])

dnl special case for BeOS
if test "${SYS}" = "beos"
then
    AX_ADD_PLUGINS([beos])
fi

dnl
dnl Skins module
dnl
AC_ARG_ENABLE(skins,
  [  --enable-skins          Skins interface module (default enabled on Win32)])
if test "${enable_skins}" != "no"; then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "cygwin"; then

    AX_ADD_PLUGINS([skins])
    AX_ADD_CPPFLAGS([skins],[-U_OFF_T_ -U_off_t -Imodules/gui/skins])
    AX_ADD_CXXFLAGS([skins],[-O2 -fno-rtti])
    AX_ADD_LDFLAGS([skins],[-loleaut32 -lwinspool -lwinmm -lshell32 -lctl3d32 -ladvapi32 -lwsock32 -lgdi32 -lcomdlg32 -lole32 -luuid -lcomctl32])

  else if test "${enable_skins}" = "yes"; then
    IMLIB2_PATH="${PATH}"
    AC_PATH_PROG(IMLIB2_CONFIG_SKINS, imlib2-config, no, ${IMLIB2_PATH})
    if test "${IMLIB2_CONFIG_SKINS}" = "no"; then
    AC_MSG_ERROR([Couldn't find the imlib2 package. You can download imlib2 from http://enlightenment.org/, or configure with --disable-skins.])
    fi

    AX_ADD_PLUGINS([skins])
    AX_ADD_CPPFLAGS([skins],[-Imodules/gui/skins -I${x_includes} `${IMLIB2_CONFIG_SKINS} --cflags` -DX11_SKINS])
    AX_ADD_CXXFLAGS([skins],[-O2 -fno-rtti])
    AX_ADD_LDFLAGS([skins],[-L${x_libraries} -lXext -lX11 `${IMLIB2_CONFIG_SKINS} --libs`])
  fi fi
fi

dnl
dnl  Gtk+ module
dnl
AC_ARG_ENABLE(gtk,
  [  --enable-gtk            Gtk+ support (default enabled)])
if test "${enable_gtk}" != "no"
then
  GTK_PATH="${PATH}"
  AC_ARG_WITH(gtk-config-path,
    [    --with-gtk-config-path=PATH gtk-config path (default search in \$PATH)],
    [ if test "${with_gtk_config_path}" != "no"
      then
        GTK_PATH="${with_gtk_config_path}:${PATH}"
      fi ])
  # look for gtk-config
  AC_PATH_PROG(GTK12_CONFIG, gtk12-config, no, ${GTK_PATH})
  GTK_CONFIG=${GTK12_CONFIG}
  if test "${GTK_CONFIG}" = "no"
  then
    AC_PATH_PROG(GTK_CONFIG, gtk-config, no, ${GTK_PATH})
  fi
  if test "${GTK_CONFIG}" != "no"
  then
    if expr 1.2.0 \> `${GTK_CONFIG} --version` >/dev/null
    then
      AC_MSG_ERROR([Your development package for Gtk+ is too old, you need at least version 1.2.0. Please upgrade and try again. Alternatively you can also configure with --disable-gtk.])
    fi
    if test "${SYS}" != "mingw32"; then
      AX_ADD_CFLAGS([gtk],[`${GTK_CONFIG} --cflags gtk gthread`])
      AX_ADD_LDFLAGS([gtk],[`${GTK_CONFIG} --libs gtk gthread | sed 's,-rdynamic,,'`])
    else
      AX_ADD_CFLAGS([gtk],[`${GTK_CONFIG} --cflags gtk`])
      AX_ADD_LDFLAGS([gtk],[`${GTK_CONFIG} --libs gtk | sed 's,-rdynamic,,'`])
    fi
    # now look for the gtk.h header
    CPPFLAGS="${CPPFLAGS_save} ${CFLAGS_gtk}"
    ac_cv_gtk_headers=yes
    AC_CHECK_HEADERS(gtk/gtk.h glib.h gdk/gdk.h, , [
      ac_cv_gtk_headers=no
      echo "Cannot find gtk development headers."
    ])
    if test "${ac_cv_gtk_headers}" = "yes"
    then
      AX_ADD_PLUGINS([gtk])
      if test "${SYS}" != "mingw32"; then
        NEED_GTK_MAIN=yes
      fi
      ALIASES="${ALIASES} gvlc"
    fi
    CPPFLAGS="${CPPFLAGS_save}"
  fi
fi

dnl
dnl  Gtk+2 module ! Disabled for know as it is unuseable and confuses users
dnl
dnl AC_ARG_ENABLE(gtk2,
dnl   [  --enable-gtk2           Gtk2 support (default disabled)])
dnl if test "${enable_gtk2}" = "yes"
dnl then
dnl   PKG_CHECK_MODULES(GTK2, [gtk+-2.0 >= 2.0.0, gthread-2.0])
dnl   AX_ADD_CFLAGS([gtk2],[${GTK2_CFLAGS}])
dnl   AX_ADD_LDFLAGS([gtk2],[${GTK2_LIBS}])
dnl   AX_ADD_PLUGINS([gtk2])
dnl   if test "${SYS}" != "mingw32"; then
dnl     NEED_GTK2_MAIN=yes
dnl   fi
dnl fi

dnl
dnl  Familiar module uses Gtk+ library
dnl
AC_ARG_ENABLE(familiar,
  [  --enable-familiar       Familiar Gtk+ support (default disabled)])
if test "${enable_familiar}" = "yes"
then
  GTK_PATH="${PATH}"
  AC_ARG_WITH(gtk-config-path,
    [    --with-gtk-config-path=PATH gtk-config path (default search in \$PATH)],
    [ if test "${with_gtk_config_path}" != "no"
      then
        GTK_PATH="${with_gtk_config_path}:${PATH}"
      fi ])
  # look for gtk-config
  AC_PATH_PROG(GTK12_CONFIG, gtk12-config, no, ${GTK_PATH})
  GTK_CONFIG=${GTK12_CONFIG}
  if test "${GTK_CONFIG}" = "no"
  then
    AC_PATH_PROG(GTK_CONFIG, gtk-config, no, ${GTK_PATH})
  fi
  # check for cross-compiling
  GTK_PREFIX=
  AC_ARG_WITH(gtk-prefix,
    [    --with-gtk-prefix=PATH path to libgtk (needed for cross-compiling),
                          e.g use as:
                          --with-gtk-prefix=/usr/local/arm/2.95.3/arm-linux/usr)],[],[])
  if test "${with_gtk_prefix}" != "no" -a -n "${with_gtk_prefix}"
  then
    GTK_PREFIX="--prefix=${with_gtk_prefix}"
  fi
  if test "${GTK_CONFIG}" != "no"
  then
    if expr 1.2.0 \> `${GTK_CONFIG} --version` >/dev/null
    then
      AC_MSG_ERROR([Your development package for Gtk+ is too old, you need at least version 1.2.0. Please upgrade and try again. Alternatively you can also configure with --disable-familiar.])
    fi
    AX_ADD_CFLAGS([familiar],[`${GTK_CONFIG} ${GTK_PREFIX} --cflags gtk gthread`])
    AX_ADD_LDFLAGS([familiar],[`${GTK_CONFIG} ${GTK_PREFIX} --libs gtk gthread | sed 's,-rdynamic,,'`])
    # now look for the gtk.h header
    CPPFLAGS="${CPPFLAGS_save} ${CFLAGS_familiar}"
    ac_cv_gtk_headers=yes
    AC_CHECK_HEADERS(gtk/gtk.h glib.h gdk/gdk.h, , [
      ac_cv_gtk_headers=no
      echo "Cannot find gtk development headers."
    ])
    if test "${ac_cv_gtk_headers}" = "yes"
    then
      AX_ADD_PLUGINS([familiar])
    fi
    CPPFLAGS="${CPPFLAGS_save}"

    # now look for gpe support
    AC_ARG_WITH(gpe-prefix,
    [    --with-gpe-prefix=PATH gpe installation path prefix (default search in \$PATH)],[],[])
    if test "${with_gpe_prefix}" != "no" -a -n "${with_gpe_prefix}"
    then
      AX_ADD_CFLAGS([gpe],[-I${with_gpe_prefix}/include])
      AX_ADD_LDFLAGS([gpe],[-lXi -lgdk_pixbuf -L${with_gpe_prefix}/lib -lgpewidget])
      # now look for gpe/init.h header file
      CFLAGS_save="${CFLAGS}"
      LDFLAGS_save="${LDFLAGS}"
      CFLAGS="${CFLAGS_familiar} ${CFLAGS_gpe}"
      LDFLAGS="${LDFLAGS_familiar} ${LDFLAGS_gpe}"
      CPPFLAGS="${CPPFLAGS_save} ${CFLAGS_familiar} ${CFLAGS_gpe}"
      ac_cv_gpe_headers=yes
      AC_CHECK_HEADERS(gpe/init.h, ,
        [ ac_cv_gpe_headers=no
          AC_MSG_ERROR([Cannot find development headers for libgpewidget...]) ])
      CFLAGS="${CFLAGS_save}"
      LDFLAGS="${LDFLAG_save}"
      if test "${ac_cv_gpe_headers}" = "yes"
      then
        AX_ADD_CFLAGS([familiar],[${CFLAGS_gpe}])
        AX_ADD_LDFLAGS([familiar],[${LDFLAGS_gpe}])
      fi
    else
	  NEED_GTK_MAIN=yes
    fi # end gpe support
  fi # end gtk+ support
fi

dnl
dnl  Gnome module
dnl
AC_ARG_ENABLE(gnome,
  [  --enable-gnome          Gnome interface support (default disabled)],
  [if test "${enable_gnome}" = "yes"; then
    # look for gnome-config
    AC_PATH_PROG(GNOME_CONFIG, gnome-config, no)
    if test -x ${GNOME_CONFIG}
    then
       AX_ADD_CFLAGS([gnome],[`${GNOME_CONFIG} --cflags gtk gnomeui`])
       AX_ADD_LDFLAGS([gnome],[`${GNOME_CONFIG} --libs gnomeui | sed 's,-rdynamic,,'`])
    fi
    # now look for the gnome.h header
    CPPFLAGS="${CPPFLAGS_save} ${CFLAGS_gnome}"
    AC_CHECK_HEADERS(gnome.h, [
      AX_ADD_PLUGINS([gnome])
      NEED_GTK_MAIN=yes
      NEED_GNOME_MAIN=yes
      ALIASES="${ALIASES} gnome-vlc"
      dnl We need this because of some moronic gnomesupport.h flavours
      AC_MSG_CHECKING(for strndup in gnome.h)
      AC_EGREP_HEADER(strndup,gnome.h,[
        AC_MSG_RESULT(yes)
        AC_DEFINE(STRNDUP_IN_GNOME_H, 1,
                  Define if <gnome.h> defines strndup.)],[
        AC_MSG_RESULT(no)])
     ],[
      AC_MSG_ERROR([Can't find gnome headers. Please install the gnome
developement tools or remove the --enable-gnome option])
     ])
    CPPFLAGS="${CPPFLAGS_save}"
  fi])

dnl
dnl  Gnome2 module ! Disabled for know as it is unuseable and confuses users
dnl
dnl AC_ARG_ENABLE(gnome2,
dnl   [  --enable-gnome2         Gnome2 support (default disabled)])
dnl if test "${enable_gnome2}" = "yes"
dnl then
dnl   PKG_CHECK_MODULES(GNOME2, [libgnomeui-2.0])
dnl   AX_ADD_CFLAGS([gnome2],[${GNOME2_CFLAGS}])
dnl   AX_ADD_LDFLAGS([gnome2],[${GNOME2_LIBS}])
dnl   AX_ADD_PLUGINS([gnome2])
dnl   if test "${SYS}" != "mingw32"; then
dnl     NEED_GNOME2_MAIN=yes
dnl   fi
dnl fi

dnl
dnl  wxWindows module
dnl
AC_ARG_ENABLE(wxwindows,
  [  --enable-wxwindows      wxWindows support (default enabled)])
if test "${enable_wxwindows}" != "no"
then
  WXWINDOWS_PATH="${PATH}"
  AC_ARG_WITH(wx-config-path,
    [    --with-wx-config-path=PATH wx-config path (default search in \$PATH)],
    [ if test "${with_wx_config_path}" != "no"
      then
        WXWINDOWS_PATH="${with_wx_config_path}:${PATH}"
      fi ])
  # look for wx-config
  AC_PATH_PROG(WX_CONFIG, wx-config, no, ${WXWINDOWS_PATH})
  if test "${WX_CONFIG}" != "no"
  then
    if expr 2.3.0 \> `${WX_CONFIG} --version` >/dev/null
    then
      AC_MSG_ERROR([Your development package for wxWindows is too old, you need at least version 2.3.0. Please upgrade and try again. Alternatively you can also configure with --disable-wxwindows.])
    fi
    AX_ADD_CXXFLAGS([wxwindows],[`${WX_CONFIG} --cxxflags`])
    AX_ADD_LDFLAGS([wxwindows],[`${WX_CONFIG} --libs`])
    # now look for the wxprec.h header
    CPPFLAGS="${CPPFLAGS_save} ${CXXFLAGS_wxwindows}"
    ac_cv_wx_headers=yes
    AC_CHECK_HEADERS(wx/wxprec.h, , [
      ac_cv_wx_headers=no
      echo "Cannot find wxWindows development headers."
    ])
    if test "${ac_cv_wx_headers}" = "yes"
    then
      AX_ADD_PLUGINS([wxwindows])
      ALIASES="${ALIASES} wxvlc"
    fi
    CPPFLAGS="${CPPFLAGS_save}"
  fi
fi

dnl
dnl  Qt module
dnl
AC_ARG_ENABLE(qt,
  [  --enable-qt             Qt interface support (default disabled)],
  [if test "${enable_qt}" = "yes"; then
     AX_ADD_PLUGINS([qt])
     ALIASES="${ALIASES} qvlc"
     AX_ADD_LDFLAGS([qt],[-L${QTDIR}/lib])
     LDFLAGS="${LDFLAGS_save} ${LDFLAGS_qt}"
     AC_CHECK_LIB(qt-mt,main,[
       AX_ADD_LDFLAGS([qt],[-lqt-mt])
     ],[
       AC_CHECK_LIB(qt,main,[
         AX_ADD_LDFLAGS([qt],[-lqt])
       ])
     ])
     LDFLAGS="${LDFLAGS_save}"
     AX_ADD_CXXFLAGS([qt],[-I/usr/include/qt3 -I/usr/include/qt -I${QTDIR}/include])
     if test -x ${QTDIR}/bin/moc
     then
       MOC=${QTDIR}/bin/moc
     else
       MOC=moc
     fi
   fi])

dnl
dnl  KDE module
dnl
AC_ARG_ENABLE(kde,
  [  --enable-kde            KDE interface support (default disabled)],
  [if test "${enable_kde}" = "yes"; then
     AX_ADD_PLUGINS([kde])
     ALIASES="${ALIASES} kvlc"
     AX_ADD_LDFLAGS([kde],[-L${KDEDIR}/lib])
     dnl Check for -lkfile (only in KDE 2) or -lkdeui -lkio (KDE 3)
     LDFLAGS="${LDFLAGS_save} ${LDFLAGS_kde}"
     AC_CHECK_LIB(kfile,main,[
       AX_ADD_LDFLAGS([kde],[-lkfile])
     ])
     LDFLAGS="${LDFLAGS_save} ${LDFLAGS_kde}"
     AC_CHECK_LIB(kdeui,main,[
       AX_ADD_LDFLAGS([kde],[-lkdeui])
     ])
     LDFLAGS="${LDFLAGS_save} ${LDFLAGS_kde}"
     AC_CHECK_LIB(kio,main,[
       AX_ADD_LDFLAGS([kde],[-lkio])
     ])
     LDFLAGS="${LDFLAGS_save}"
     AX_ADD_CXXFLAGS([kde],[-I/usr/include/kde -I/usr/include/qt3 -I/usr/include/qt])
     AX_ADD_CXXFLAGS([kde],[-I${KDEDIR}/include -I${QTDIR}/include])
     if test -x ${QTDIR}/bin/moc
     then
       MOC=${QTDIR}/bin/moc
     else
       MOC=moc
     fi
   fi])

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
       AX_ADD_LDFLAGS([qte],[-L${with_qte}/lib `echo -L${with_qte}/lib | sed 's,opt/QtPalmtop,usr,'` -ljpeg -lqte])
       AX_ADD_CXXFLAGS([qte],[-I${with_qte}/include `echo -I${with_qte}/include | sed 's,opt/QtPalmtop,usr,'` -DQT_QWS_IPAQ -DQWS -fno-exceptions -fno-rtti])
     else
       AX_ADD_LDFLAGS([qte],[-L${QTDIR}/lib `echo -L${QTDIR}/lib | sed 's,opt/QtPalmtop,usr,'`])
       AX_ADD_CXXFLAGS([qte],[-I${QTDIR}/include `echo -I${QTDIR}/include | sed 's,opt/QtPalmtop,usr,'`])
     fi
     CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_qte}"
     AC_CHECK_HEADERS(qt.h jpeglib.h, ,[
       AC_MSG_ERROR([echo "Cannot find QT Embedded development headers."])
     ] )
     CPPFLAGS="${CPPFLAGS_save}"

     AX_ADD_PLUGINS([opie])
     NEED_QTE_MAIN=yes
     AX_ADD_LDFLAGS([opie],[-lqpe ${LDFLAGS_qte}])
     AX_ADD_CXXFLAGS([opie],[${CXXFLAGS_qte}])
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
     AX_ADD_BUILTINS([macosx])
     AX_ADD_LDFLAGS([macosx],[-framework IOKit -framework Cocoa -framework Carbon -framework QuickTime -lobjc -ObjC])
   fi],
  [AC_CHECK_HEADERS(Cocoa/Cocoa.h,
     AX_ADD_BUILTINS([macosx])
     AX_ADD_LDFLAGS([macosx],[-framework IOKit -framework Cocoa -framework Carbon -framework QuickTime -lobjc -ObjC])
   )])

dnl
dnl  QNX RTOS module
dnl
AC_ARG_ENABLE(qnx,
  [  --enable-qnx            QNX RTOS support (default enabled on QNX RTOS)])
    if test "${enable_qnx}" != "no"
    then
      AC_CHECK_HEADERS(Ph.h, [
        AX_ADD_PLUGINS([qnx])
        AX_ADD_LDFLAGS([qnx],[-lasound -lph])
      ])
    fi

dnl
dnl  ncurses module
dnl
AC_ARG_ENABLE(ncurses,
  [  --enable-ncurses        ncurses interface support (default disabled)],
  [if test "${enable_ncurses}" = "yes"; then
     AX_ADD_PLUGINS([ncurses])
     AX_ADD_LDFLAGS([ncurses],[-lncurses])
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
    AX_ADD_PLUGINS([xosd])
    AX_ADD_LDFLAGS([xosd],[-lxosd])
  fi
fi

dnl
dnl  SLP access plugin
dnl
AC_ARG_ENABLE(slp,
  [  --enable-slp            SLP service discovery support (default enabled)])
if test "${enable_slp}" != "no"
then
  AC_ARG_WITH(slp,
  [    --with-slp=PATH       libslp headers and libraries])
  if test -z "${with_slp}"
  then
    AC_CHECK_HEADER(slp.h, have_slp="true", have_slp="false")
    if test "${have_slp}" = "true"
    then
      AX_ADD_PLUGINS([slp])
      AX_ADD_LDFLAGS([slp],[-lslp])
    fi
  else
    AC_MSG_CHECKING(for slp headers in ${with_slp})
    if test -f ${with_slp}/slp.h
    then
      dnl  Use ${with_slp}/libslp/slp.h
      AC_MSG_RESULT(yes)
      AX_ADD_PLUGINS([slp])
      AX_ADD_LDFLAGS([slp],[-L${with_slp} -lslp])
      AX_ADD_CPPFLAGS([slp],[-I${with_slp}])
    else
      dnl  No libslp could be found, sorry
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${with_slp}/slp.h])
    fi
  fi
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
    AX_ADD_PLUGINS([lirc])
    AX_ADD_LDFLAGS([lirc],[-llirc_client])
  fi
fi

dnl 
dnl  Joystick plugin
dnl
AC_ARG_ENABLE(joystick,
  [  --enable-joystick       joystick control (default disabled)])
if test "${enable_joystick}" = "yes"
then
 AC_CHECK_HEADER( linux/joystick.h, 
		[AX_ADD_PLUGINS([joystick])]
	        )
fi


dnl
dnl corba (ORBit) plugin
dnl
AC_ARG_ENABLE(corba,
  [  --enable-corba          corba interface support (default disabled)])
if test "${enable_corba}" = "yes"; then
  ORBIT_PATH="${PATH}"
  AC_ARG_WITH(orbit-config-path,
  [    --with-orbit-config-path=PATH orbit-config path (default search in \$PATH)])
  if test "${with_orbit_config_path}" != "no"; then
    ORBIT_PATH="${with_orbit_config_path}:${PATH}"
  fi
  # look for orbit2-config
  AC_PATH_PROG(ORBIT_CONFIG, orbit2-config, no, ${ORBIT_PATH})
  if test "${ORBIT_CONFIG}" != "no"; then
    AX_ADD_CFLAGS(corba,[`${ORBIT_CONFIG} --cflags server`])
    AX_ADD_LDFLAGS(corba,[`${ORBIT_CONFIG} --libs server | sed 's,-rdynamic,,'`])
    # now look for the orbit.h header
    CPPFLAGS="${CPPFLAGS_save} ${CFLAGS_corba}"
    ac_cv_corba_headers=yes
    AC_CHECK_HEADERS(orbit/orbit.h, , [
      ac_cv_corba_headers=no
      AC_MSG_ERROR([Could not find corba development headers])
    ])
    if test "${ac_cv_corba_headers}" = "yes"; then
      AX_ADD_PLUGINS(corba)
    fi
    CPPFLAGS="${CPPFLAGS_save}"
  fi
fi

AC_ARG_WITH(,[Misc options:])

dnl
dnl  Endianness check, AC_C_BIGENDIAN doesn't work if we are cross-compiling
dnl
dnl  We give the user the opportunity to specify
dnl  --with-words=big or --with-words=little ; otherwise, try to guess
dnl
AC_ARG_WITH(words,
  [  --with-words=endianness set endianness (big or little)])
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
dnl  Mozilla plugin
dnl
mozilla=false
AC_ARG_ENABLE(mozilla,
  [  --enable-mozilla        build a vlc-based Mozilla plugin (default disabled)])
if test "${enable_mozilla}" = "yes"
then
  AC_PATH_PROG(MOZILLA_CONFIG, mozilla-config, no)
  if test "${MOZILLA_CONFIG}" = "no"
  then
    AC_MSG_ERROR([Please install the Mozilla development tools, mozilla-config was not found.])
  else
    if test "${SYS}" != "mingw32"; then
      LDFLAGS="${LDFLAGS_save} -L${x_libraries}"
      AC_CHECK_LIB(Xt,XtStrings,[
        AX_ADD_LDFLAGS([mozilla],[-L${x_libraries} -lXt])
      ])
      LDFLAGS="${LDFLAGS_save}"
    fi
    mozilla=:
    dnl Workaround for http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=150490
    AX_ADD_CPPFLAGS([mozilla],[[`${MOZILLA_CONFIG} --cflags plugin xpcom java | sed 's,-I\([^ ]*\)/mozilla/\([^ ]*\),-I\1/\2 -I\1/mozilla/\2,g' | xargs`]])
    AX_ADD_LDFLAGS([mozilla],[`${MOZILLA_CONFIG} --libs plugin xpcom`])
    dnl Not necessarily in ${PATH}
    if test -z "${XPIDL}" -o ! -x "${XPIDL}"
    then
      XPIDL="/usr/lib/mozilla/xpidl"
    fi
  fi
fi
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
  AX_ADD_CXXFLAGS([test2],[])
  AX_ADD_OBJCFLAGS([test3],[])
  dnl  this one is needed until automake knows what to do
  AX_ADD_LDFLAGS([test3],[-lobjc])

  AX_ADD_PLUGINS([${TESTS}])
  #AX_ADD_BUILTINS([${TESTS}])
fi

dnl
dnl  gtk_main plugin
dnl
if test "${NEED_GTK_MAIN}" != "no"
then
    AX_ADD_PLUGINS([gtk_main])
    AX_ADD_CFLAGS([gtk familiar],[-DNEED_GTK_MAIN])
    AX_ADD_CFLAGS([gtk_main],[${CFLAGS_gtk} ${CFLAGS_familiar}])
    AX_ADD_LDFLAGS([gtk_main],[${LDFLAGS_gtk} ${LDFLAGS_familiar}])
fi

if test "${NEED_GNOME_MAIN}" != "no"
then
    AX_ADD_PLUGINS([gnome_main])
    AX_ADD_CFLAGS([gnome_main],[${CFLAGS_gtk} ${CFLAGS_familiar} ${CFLAGS_gnome}])
    AX_ADD_LDFLAGS([gnome_main],[${LDFLAGS_gtk} ${LDFLAGS_familiar} ${LDFLAGS_gnome}])
fi

if test "${NEED_GTK2_MAIN}" != "no"
then
    AX_ADD_PLUGINS([gtk2_main])
    AX_ADD_CFLAGS([gtk2],[-DNEED_GTK2_MAIN])
    AX_ADD_CFLAGS([gtk2_main],[${CFLAGS_gtk2}])
    AX_ADD_LDFLAGS([gtk2_main],[${LDFLAGS_gtk2}])
fi

if test "${NEED_GNOME2_MAIN}" != "no"
then
    AX_ADD_PLUGINS([gnome2_main])
    AX_ADD_CFLAGS([gnome2_main],[${CFLAGS_gtk2} ${CFLAGS_gnome2}])
    AX_ADD_LDFLAGS([gnome2_main],[${LDFLAGS_gtk2} ${LDFLAGS_gnome2}])
fi

dnl
dnl  qte_main plugin
dnl
if test "${NEED_QTE_MAIN}" != "no"
then
    AX_ADD_PLUGINS([qte_main])
    AX_ADD_CPPFLAGS([qte opie vout_sdl],[-DNEED_QTE_MAIN])
    AX_ADD_CXXFLAGS([qte_main],[${CPPFLAGS_qte} ${CXXFLAGS_qte}])
    AX_ADD_LDFLAGS([qte_main],[${LDFLAGS_qte}])
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
if test "${ac_cv_header_dlfcn_h}" = "no" -a "${ac_cv_header_image_h}" = "no" -a "${SYS}" != "mingw32"
then
  echo "*** Your system doesn't have plugin support. All plugins will be built"
  echo "statically."
  plugin_support=false
fi

dnl Export automake variables
if ${plugin_support}
then
  for plugin in `echo ${PLUGINS}`
  do
    eval "${plugin}_p=yes"
  done
else
  AX_ADD_BUILTINS([${PLUGINS}])
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
AX_VLC_CONDITIONALS

dnl
dnl  Stuff used by the program
dnl
AC_DEFINE_UNQUOTED(VERSION_MESSAGE, "${VERSION} ${CODENAME}", [Simple version string])
AC_DEFINE_UNQUOTED(COPYRIGHT_MESSAGE, "VLC media player - version ${VERSION} ${CODENAME} - (c) 1996-2003 VideoLAN", [Copyright string])
AC_DEFINE_UNQUOTED(CONFIGURE_LINE, "${CONFIGURE_LINE}", [The ./configure command line])

VLC_SYMBOL="`echo ${VERSION} | sed -e 'y/.-/__/'`"
AC_DEFINE_UNQUOTED(MODULE_SUFFIX, "__${VLC_SYMBOL}", [String suffix for module functions])
AC_DEFINE_UNQUOTED(MODULE_SYMBOL, ${VLC_SYMBOL}, [Symbol suffix for module functions])
AC_DEFINE_UNQUOTED(LIBEXT, "${LIBEXT}", [Dynamic object extension])

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
AX_RESTORE_FLAGS

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
AC_SUBST(LIBEXT)
AC_SUBST(INCLUDES)
AC_SUBST(ALL_LINGUAS)

dnl Import substitutions generated by bootstrap
AX_VLC_SUBSTS

dnl Create vlc-config.in
AX_OUTPUT_VLC_CONFIG_IN

AC_OUTPUT([
  vlc-config

  Makefile
  autotools/Makefile
  debian/Makefile
  doc/Makefile
  intl/Makefile
  ipkg/Makefile
  lib/Makefile
  modules/Makefile
  mozilla/Makefile
  m4/Makefile
  po/Makefile.in
  share/Makefile
  src/Makefile

  modules/access/Makefile
  modules/access/dvd/Makefile
  modules/access/dvdplay/Makefile
  modules/access/dvdread/Makefile
  modules/access/mms/Makefile
  modules/access/pvr/Makefile
  modules/access/satellite/Makefile
  modules/access/v4l/Makefile
  modules/access/vcd/Makefile
  modules/access_output/Makefile
  modules/audio_filter/Makefile
  modules/audio_filter/channel_mixer/Makefile
  modules/audio_filter/converter/Makefile
  modules/audio_filter/resampler/Makefile
  modules/audio_mixer/Makefile
  modules/audio_output/Makefile
  modules/codec/Makefile
  modules/codec/a52old/Makefile
  modules/codec/a52old/downmix/Makefile
  modules/codec/a52old/imdct/Makefile
  modules/codec/cinepak/Makefile
  modules/codec/faad/Makefile
  modules/codec/ffmpeg/Makefile
  modules/codec/ffmpeg/postprocessing/Makefile
  modules/codec/mpeg_audio/Makefile
  modules/codec/mpeg_video/Makefile
  modules/codec/mpeg_video/idct/Makefile
  modules/codec/mpeg_video/motion/Makefile
  modules/codec/spudec/Makefile
  modules/control/Makefile
  modules/control/corba/Makefile
  modules/control/lirc/Makefile
  modules/control/rc/Makefile
  modules/demux/Makefile
  modules/demux/aac/Makefile
  modules/demux/asf/Makefile
  modules/demux/avi/Makefile
  modules/demux/mp4/Makefile
  modules/demux/mpeg/Makefile
  modules/demux/util/Makefile
  modules/demux/wav/Makefile
  modules/encoder/Makefile
  modules/encoder/ffmpeg/Makefile
  modules/gui/Makefile
  modules/gui/beos/Makefile
  modules/gui/familiar/Makefile
  modules/gui/gtk/Makefile
  modules/gui/gtk2/Makefile
  modules/gui/kde/Makefile
  modules/gui/macosx/Makefile
  modules/gui/ncurses/Makefile
  modules/gui/qnx/Makefile
  modules/gui/qt/Makefile
  modules/gui/skins/Makefile
  modules/gui/win32/Makefile
  modules/gui/wxwindows/Makefile
  modules/misc/Makefile
  modules/misc/dummy/Makefile
  modules/misc/logger/Makefile
  modules/misc/memcpy/Makefile
  modules/misc/network/Makefile
  modules/misc/testsuite/Makefile
  modules/mux/Makefile
  modules/mux/mpeg/Makefile
  modules/packetizer/Makefile
  modules/stream_out/Makefile
  modules/video_chroma/Makefile
  modules/video_filter/Makefile
  modules/video_filter/deinterlace/Makefile
  modules/video_output/Makefile
  modules/video_output/directx/Makefile
  modules/video_output/mga/Makefile
  modules/video_output/qte/Makefile
  modules/video_output/x11/Makefile
  modules/visualization/Makefile
  modules/visualization/scope/Makefile
  modules/visualization/xosd/Makefile
],[
  chmod 0755 vlc-config
])

dnl Generate makefiles found by bootstrap

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

