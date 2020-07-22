dnl Autoconf settings for vlc

AC_COPYRIGHT([Copyright 1999-2020 VLC authors and VideoLAN])

AC_INIT([vlc], [4.0.0-dev])
VERSION_MAJOR=4
VERSION_MINOR=0
VERSION_REVISION=0
VERSION_EXTRA=0
VERSION_DEV=dev

PKGDIR="vlc"
AC_SUBST([PKGDIR])

CONFIGURE_LINE="`echo "$0 $ac_configure_args" | sed -e 's/\\\/\\\\\\\/g'`"
CODENAME="Otto Chriek"
COPYRIGHT_YEARS="1996-2020"

AC_CONFIG_SRCDIR([src/libvlc.c])
AC_CONFIG_AUX_DIR([autotools])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_LIBOBJ_DIR([compat])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_PRESERVE_HELP_ORDER

AM_INIT_AUTOMAKE([tar-ustar color-tests foreign])
AC_CONFIG_HEADERS([config.h])

AM_SILENT_RULES([yes])
AM_MAINTAINER_MODE([enable])

dnl
dnl Check for tools
dnl
AC_PROG_CC
AC_USE_SYSTEM_EXTENSIONS
VLC_PROG_CC_C11
AH_VERBATIM([FORTIFY_SOURCE],
  [/* Enable compile-time and run-time bounds-checking, and some warnings,
   without upsetting glibc 2.15+ or toolchains predefining _FORTIFY_SOURCE */
#if !defined _FORTIFY_SOURCE && defined __OPTIMIZE__ && __OPTIMIZE__
# define _FORTIFY_SOURCE 2
#endif])
AC_DEFINE([_FILE_OFFSET_BITS], 64, [Define to 64 for large files support.])
AC_DEFINE([__LIBVLC__],, [Define within the LibVLC source code tree.])
AC_DEFINE([WIN32_LEAN_AND_MEAN],, [Define to limit the scope of <windows.h>.])

AC_PROG_CXX
AC_PROG_CPP
AC_PROG_OBJC
AC_PROG_EGREP
AC_PROG_MAKE_SET
AC_PROG_INSTALL
AM_PROG_AS
AC_PROG_LEX
AC_PROG_YACC

AC_ARG_VAR([DESKTOP_FILE_VALIDATE], [Validator for desktop entry files])
AC_CHECK_PROGS([DESKTOP_FILE_VALIDATE], [${DESKTOP_FILE_VALIDATE} desktop-file-validate], :)
AC_ARG_VAR([BUILDCC], [Build system C11 or C99 compiler command])
AC_CHECK_PROGS([BUILDCC], [c11-gcc c11 c99-gcc c99], [false])
AS_IF([test "$BUILDCC" = "false"], [
  AC_MSG_ERROR([Cannot find native C99 compiler: please define BUILDCC.])
])

dnl Check for compiler properties
AC_C_CONST
AC_C_INLINE
AC_C_RESTRICT
AX_CXX_COMPILE_STDCXX_14([noext], [mandatory])

dnl Check the compiler supports atomics in C
AC_MSG_CHECKING([C atomics])
VLC_SAVE_FLAGS
ATOMIC_LIB=""
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <stdatomic.h>],[
 atomic_uintmax_t test;
 atomic_init(&test, 0);
 atomic_fetch_add_explicit(&test, 2u, memory_order_relaxed);
])], [AC_MSG_RESULT([built-in])], [
  LDFLAGS="$LDFLAGS -latomic"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <stdatomic.h>],[
atomic_uintmax_t test;
atomic_init(&test, 0);
atomic_fetch_add_explicit(&test, 2u, memory_order_relaxed);
])],[
    AC_MSG_RESULT([using -latomic])
    ATOMIC_LIB="-latomic"
  ],[
  AC_MSG_ERROR([VLC requires atomics support])
  ])
])
VLC_RESTORE_FLAGS
AC_SUBST([ATOMIC_LIB])

dnl Extend the --help string at the current spot.
AC_DEFUN([EXTEND_HELP_STRING], [m4_divert_once([HELP_ENABLE], [$1])])

dnl Allow binary package maintainer to pass a custom string to avoid
dnl cache problem
AC_ARG_WITH([binary-version],
    AS_HELP_STRING([--with-binary-version=STRING],
         [To avoid plugins cache problem between binary version]),[],[])
AS_IF([test -n "${with_binary_version}"],[
    AC_DEFINE_UNQUOTED([DISTRO_VERSION],["${with_binary_version}"],
         [Binary specific version])
])

dnl Prevent clang from accepting unknown flags with a mere warning
AX_CHECK_COMPILE_FLAG([-Werror=invalid-command-line-argument], [
  AX_APPEND_FLAG([-Werror=invalid-command-line-argument], [CFLAGS])
  AX_APPEND_FLAG([-Werror=invalid-command-line-argument], [CXXFLAGS])
  AX_APPEND_FLAG([-Werror=invalid-command-line-argument], [OBJCFLAGS])
])
AX_CHECK_COMPILE_FLAG([-Werror=unknown-warning-option], [
  AX_APPEND_FLAG([-Werror=unknown-warning-option], [CFLAGS])
  AX_APPEND_FLAG([-Werror=unknown-warning-option], [CXXFLAGS])
  AX_APPEND_FLAG([-Werror=unknown-warning-option], [OBJCFLAGS])
])

dnl
dnl  Check the operating system
dnl
HAVE_WIN64="0"
HAVE_IOS="0"
HAVE_OSX="0"
HAVE_TVOS="0"

dnl  Set x86 asm flags and defines
X86ASMFLAGS=""
case "${host_cpu}" in
    i?86)
    X86ASMFLAGS="-f elf32"
    X86ASMDEFS="-DARCH_X86_32=1 -DARCH_X86_64=0"
        ;;
    x86_64)
    X86ASMFLAGS="-f elf64"
    X86ASMDEFS="-DARCH_X86_32=0 -DARCH_X86_64=1"
        ;;
esac

case "${host_os}" in
  "")
    SYS=unknown
    ;;
  linux*)
    SYS=linux
    CFLAGS="${CFLAGS} -pthread"
    ;;
  bsdi*)
    SYS=bsdi
    CFLAGS="${CFLAGS} -pthread"
    ;;
  freebsd*)
    SYS=freebsd
    CFLAGS="${CFLAGS} -pthread"
    CPPFLAGS="${CPPFLAGS} -I/usr/local/include"
    LDFLAGS="${LDFLAGS} -L/usr/local/lib"
    ac_cv_ld_z_defs=no
    ;;
  openbsd*)
    SYS=openbsd
    CPPFLAGS="${CPPFLAGS} -I/usr/local/include -I/usr/X11R6/include"
    LDFLAGS="${LDFLAGS} -L/usr/local/lib -L/usr/X11R6/lib/"
    ;;
  netbsd*)
    SYS=netbsd
    ;;
  dragonfly*)
    SYS=dragonfly
    ;;
  darwin*)
    SYS=darwin

    dnl Work around an Xcode 11 bug, fixed in Xcode 11.3.1
    AX_APPEND_FLAG([-fno-stack-check], [CFLAGS])
    AX_APPEND_FLAG([-fno-stack-check], [CXXFLAGS])
    AX_APPEND_FLAG([-fno-stack-check], [OBJCFLAGS])

    case "${host_cpu}" in
      i?86)
        X86ASMFLAGS="-f macho32"
        AX_APPEND_FLAG([-DPREFIX], [X86ASMDEFS])
      ;;
      x86_64)
        X86ASMFLAGS="-f macho64"
        AX_APPEND_FLAG([-DPREFIX], [X86ASMDEFS])
      ;;
      arm*)
        ac_cv_c_bigendian="no"
        ac_cv_c_attribute_packed="no"
      ;;
    esac

    AX_APPEND_FLAG([-D_INTL_REDIRECT_MACROS], [CPPFLAGS])
    AX_APPEND_FLAG([-stdlib=libc++], [CXXFLAGS])
    AX_APPEND_FLAG([-Werror=partial-availability], [OBJCFLAGS])
    AX_APPEND_FLAG([-Wl,-headerpad_max_install_names], [LDFLAGS])

    VLC_ADD_LIBS([libvlc vlc],[-Wl,-undefined,dynamic_lookup,-framework,AppKit])
    VLC_ADD_LIBS([libvlccore],[-Wl,-framework,CoreFoundation])

    AC_EGREP_CPP(yes,
            [#import <TargetConditionals.h>
             #if TARGET_OS_IPHONE
             yes
             #endif],
             [HAVE_IOS="1"],
             [HAVE_OSX="1"])

    AC_EGREP_CPP(yes,
            [#import <TargetConditionals.h>
             #if TARGET_OS_TV
             yes
             #endif],
             [HAVE_TVOS="1"
             HAVE_IOS="0"
             HAVE_OSX="0"
             ac_cv_func_fork=no
             ],)

    AS_IF([test "${HAVE_OSX}" = "1"],[VLC_ADD_LIBS([libvlccore],[-Wl,-framework,CoreServices])])

    dnl
    dnl  Handle Mac OS X SDK flags
    dnl
    AC_ARG_WITH(macosx-sdk,
      AS_HELP_STRING([--with-macosx-sdk=DIR],
        [compile using the SDK in DIR]))
    if test "${with_macosx_sdk}" != "" ; then
        test ! -d "${with_macosx_sdk}" && AC_MSG_ERROR([SDK "${with_macosx_sdk}" not found])
        CPP="${CPP} -isysroot ${with_macosx_sdk}"
        CC="${CC} -isysroot ${with_macosx_sdk}"
        CXX="${CXX} -isysroot ${with_macosx_sdk}"
        OBJC="${OBJC} -isysroot ${with_macosx_sdk}"
        LD="${LD} -syslibroot ${with_macosx_sdk}"
    fi
    AC_ARG_WITH(macosx-version-min,
      AS_HELP_STRING([--with-macosx-version-min=VERSION],
        [compile for Mac OS X VERSION and above]))
    if test "${with_macosx_version_min}" != "" ; then
        CPP="${CPP} -mmacosx-version-min=${with_macosx_version_min}"
        CC="${CC} -mmacosx-version-min=${with_macosx_version_min}"
        CXX="${CXX} -mmacosx-version-min=${with_macosx_version_min}"
        OBJC="${OBJC} -mmacosx-version-min=${with_macosx_version_min}"
        LD="${LD} -mmacosx_version_min=${with_macosx_version_min}"
    fi
    ;;
  *mingw32* | *cygwin* | *wince* | *mingwce*)
    dnl Force libtool to not link to (shared) libgcc_s when detecting C++ dependencies
    dnl When doing this test with -static-libgcc it will link on (static) libgcc_eh
    CXXFLAGS="${CXXFLAGS} -static-libgcc"
    AC_CHECK_TOOL(WINDRES, windres, :)
    AC_CHECK_TOOL(OBJCOPY, objcopy, :)
    AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
      [[#ifdef _WIN32_WINNT
       # error _WIN32_WINNT already defined
       #else
       # include <windows.h>
       # if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0601
       #  error _WIN32_WINNT toolchain default high enough
       # endif
       #endif
      ]],[[;]])
    ],[
      AC_DEFINE([_WIN32_WINNT], [0x0601], [Define to '0x0601' for Windows 7 APIs.])
    ])
    AC_MSG_CHECKING([if libsynchronization is needed])
    AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
      [[#if _WIN32_WINNT >= 0x0602
       # error building for Win8+ and above
       #endif
      ]],[[;]])
    ],[
      AC_MSG_RESULT([no])
    ],[
      AC_MSG_RESULT([yes])
      VLC_ADD_LIBS([libvlccore], [-lsynchronization])
    ])
    AC_DEFINE([_WIN32_IE], [0x0600], [Define to '0x0600' for IE 6.0 (and shell) APIs.])
    AC_DEFINE([UNICODE], [1], [Define to 1 for Unicode (Wide Chars) APIs.])
    AC_DEFINE([_ISOC99_SOURCE], [1], [Extensions to ISO C89 from ISO C99.])
    AC_DEFINE([_ISOC11_SOURCE], [1], [Extensions to ISO C99 from ISO C11.])
    AC_DEFINE([_POSIX_SOURCE], [1], [IEEE Std 1003.1.])
    AC_DEFINE([_POSIX_C_SOURCE], [200809L], [IEEE Std 1003.1.])
    AC_DEFINE([_XOPEN_SOURCE], [700], [POSIX and XPG 7th edition])
    AC_DEFINE([_XOPEN_SOURCE_EXTENDED], [1], [XPG things and X/Open Unix extensions.])
    AC_DEFINE([_BSD_SOURCE], [1], [ISO C, POSIX, and 4.3BSD things.])
    AC_DEFINE([_SVID_SOURCE], [1], [ISO C, POSIX, and SVID things.])

    case "${host_os}" in
      *mingw32*)
        SYS=mingw32
        ;;
      *cygwin*)
        dnl Check if we are using the mno-cygwin mode in which case we are
        dnl actually dealing with a mingw32 compiler.
        AC_EGREP_CPP(yes,
            [#ifdef _WIN32
             yes
             #endif],
            SYS=mingw32, AC_MSG_ERROR([VLC requires -mno-cygwin]))
        ;;
    esac

    if test "${SYS}" = "mingw32"; then
        # DEP, ASLR, NO SEH
        LDFLAGS="${LDFLAGS} -Wl,--nxcompat -Wl,--no-seh -Wl,--dynamicbase"

        AC_CHECK_PROGS(U2D, [unix2dos todos], unix2dos)
        ac_default_prefix="`pwd`/_win32"
        DESTDIR="`pwd`/_win32/"

        dnl
        dnl NSIS/MSI Installer prefix and WIN64
        dnl
        case "${host_cpu}" in
            aarch64)
                HAVE_WIN64="1"
                WINDOWS_ARCH="arm64"
                PROGRAMFILES="PROGRAMFILES64"
                LDFLAGS="${LDFLAGS} -Wl,--high-entropy-va -Wl,--image-base,0x140000000"
            ;;
            amd64|x86_64)
                HAVE_WIN64="1"
                WINDOWS_ARCH="x64"
                PROGRAMFILES="PROGRAMFILES64"
                LDFLAGS="${LDFLAGS} -Wl,--high-entropy-va -Wl,--image-base,0x140000000"
                X86ASMFLAGS="-f win64"
            ;;
            armv7)
                WINDOWS_ARCH="arm"
                PROGRAMFILES="PROGRAMFILES"
            ;;
            *)
                WINDOWS_ARCH="x86"
                PROGRAMFILES="PROGRAMFILES"
                X86ASMFLAGS="-f win32"
                X86ASMDEFS="${X86ASMDEFS} -DPREFIX"
            ;;
        esac
        AC_SUBST([WINDOWS_ARCH])
        AC_SUBST([PROGRAMFILES])

    fi
    ;;
  *nto*)
    SYS=nto
    ;;
  solaris*)
    SYS=solaris
    ;;
  hpux*)
    SYS=hpux
    ;;
  *os2*)
    SYS=os2
    LDFLAGS="${LDFLAGS} -Zomf -Zbin-files -Zargs-wild -Zhigh-mem"
    AC_LIBOBJ([freeaddrinfo])
    AC_LIBOBJ([gai_strerror])
    AC_CHECK_FUNCS([if_nametoindex])
    X86ASMFLAGS="-f aout"
    X86ASMDEFS="-DARCH_X86_32=1 -DARCH_X86_64=0 -DPREFIX"
    ;;
  *nacl*)
    SYS=nacl
    AC_DEFINE([_XOPEN_SOURCE], [700], [POSIX and XPG 7th edition])
    AC_LIBOBJ([sigwait])
    AC_LIBOBJ([recvmsg])
    AC_LIBOBJ([sendmsg])
    ;;
  *)
    SYS="${host_os}"
    ;;
esac

dnl WinRT
AC_ARG_ENABLE([winstore_app],
     AS_HELP_STRING([--enable-winstore-app],
                    [Build targetted for Windows Store apps (default disabled)]))

vlc_winstore_app=0
AS_IF([test "${SYS}" = "mingw32"],[
  LIBCOM="-lole32"
  AS_IF([test "${enable_winstore_app}" = "yes"], [
    vlc_winstore_app=1
    LIBCOM=""
    AX_APPEND_FLAG([-DWINSTORECOMPAT],[CFLAGS])
    AX_APPEND_FLAG([-DWINSTORECOMPAT],[CPPFLAGS])
    AX_APPEND_FLAG([-DWINSTORECOMPAT],[CXXFLAGS])
    LDFLAGS="${LDFLAGS} -lwindowsappcompat"
    VLC_ADD_LIBS([libvlccore], [-lruntimeobject])
    AC_LIBOBJ([gai_strerror])
  ],[])
  AC_SUBST([LIBCOM])
  ])
AC_DEFINE_UNQUOTED(VLC_WINSTORE_APP, ${vlc_winstore_app}, [Define to 1 if you want to build for Windows Store apps])

vlc_build_pdb=0
AC_ARG_ENABLE([pdb],
     AS_HELP_STRING([--enable-pdb],
                    [Build PDB files for windows targets (default off)]))
AS_IF([test "${SYS}" = "mingw32"],[
  AS_IF([test "${enable_pdb}" = "yes"], [
    vlc_build_pdb=1
    AX_APPEND_FLAG([-g -gcodeview],[CFLAGS])
    AX_APPEND_FLAG([-g -gcodeview],[CXXFLAGS])
    LDFLAGS="${LDFLAGS} -Wl,-pdb="
  ],[])
])

AM_CONDITIONAL([HAVE_FREEBSD], [test "${SYS}" = "freebsd"])
AM_CONDITIONAL([HAVE_LINUX],   [test "${SYS}" = "linux"])
AM_CONDITIONAL([HAVE_OS2],     [test "${SYS}" = "os2"])

AM_CONDITIONAL([HAVE_DARWIN],  [test "${SYS}" = "darwin"])
AM_CONDITIONAL([HAVE_IOS],     [test "${HAVE_IOS}" = "1"])
AM_CONDITIONAL([HAVE_OSX],     [test "${HAVE_OSX}" = "1"])
AM_CONDITIONAL([HAVE_TVOS],    [test "${HAVE_TVOS}" = "1"])

AM_CONDITIONAL([HAVE_NACL],    [test "${SYS}" = "nacl"])
AM_CONDITIONAL([HAVE_LIBANL],  [test "${HAVE_LIBANL}" = "1"])

AM_CONDITIONAL([HAVE_WIN32],   [test "${SYS}" = "mingw32"])
AM_CONDITIONAL([HAVE_WIN64],   [test "${HAVE_WIN64}" = "1"]) dnl Only used for the packaging
AM_CONDITIONAL([HAVE_WINSTORE], [test "$vlc_winstore_app" = "1"])
AM_CONDITIONAL([HAVE_WIN32_DESKTOP], [test "${SYS}" = "mingw32" -a "$vlc_winstore_app" = "0"])
AM_CONDITIONAL([ENABLE_PDB],   [test "$vlc_build_pdb" = "1"])

dnl Use nasm/yasm only on x86
AC_CHECK_PROGS(X86ASM, [nasm yasm])
AM_CONDITIONAL([HAVE_X86ASM], [test -n "${X86ASM}" && test -n "${X86ASMFLAGS}"])
AM_COND_IF([HAVE_X86ASM], [
  AC_DEFINE([HAVE_X86ASM], [1], [Use external asm on x86.])
])

dnl  Because DOLT is not able to handle --with-pic
dnl  correctly, manually add -DPIC to the options for
dnl  nasm / yasm here.
AS_IF([test x$with_pic = xyes], [X86ASMDEFS="${X86ASMDEFS} -DPIC"])

AC_SUBST([X86ASMFLAGS])
AC_SUBST([X86ASMDEFS])

dnl
dnl Sadly autoconf does not think about testing foo.exe when ask to test
dnl for program foo on win32
case "${build_os}" in
    cygwin|msys|mingw32)
        ac_executable_extensions=".exe"
        BUILDEXEEXT=".exe"
    ;;
    os2*)
        BUILDEXEEXT=".exe"
    ;;
    *)
    ;;
esac
AC_SUBST([BUILDEXEEXT])

dnl Android is linux, but a bit different
AS_IF([test "$SYS" = linux],[
    AC_MSG_CHECKING([for an Android system])
    AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
        [[#ifndef __ANDROID__
         # error Not Android
         #endif
        ]],[[;]])
    ],[
      HAVE_ANDROID="1"
      AC_MSG_RESULT([yes])
    ],[
      AC_MSG_RESULT([no])
    ])
])
AM_CONDITIONAL([HAVE_ANDROID], [test "${HAVE_ANDROID}" = "1"])

dnl
dnl  Check for the contrib directory
dnl
AC_ARG_WITH(contrib,
  AS_HELP_STRING([--with-contrib[=DIR]],
    [search for 3rd party libraries in DIR/include and DIR/lib])
)
AC_MSG_CHECKING([for 3rd party libraries path])
AS_IF([test -z "${with_contrib}" || test "${with_contrib}" = "yes"], [
  CONTRIB_DIR="${srcdir}/contrib/${host}"
  AS_IF([test ! -d "${CONTRIB_DIR}"], [
    echo "${CONTRIB_DIR} not found" >&AS_MESSAGE_LOG_FD
    CONTRIB_DIR="${srcdir}/contrib/`$CC -dumpmachine`"
    AS_IF([test ! -d "${CONTRIB_DIR}"], [
      echo "${CONTRIB_DIR} not found" >&AS_MESSAGE_LOG_FD
      CONTRIB_DIR=""
      AC_MSG_RESULT([not found])
    ])
  ])
], [
  AS_IF([test "${with_contrib}" != "no"], [
    CONTRIB_DIR="${with_contrib}"
  ], [
    CONTRIB_DIR=""
    AC_MSG_RESULT([disabled])
  ])
])
AS_IF([test -n "${CONTRIB_DIR}"], [
  AS_IF([test -d "${CONTRIB_DIR}/lib"],[
    CONTRIB_DIR=`cd "${CONTRIB_DIR}" && pwd`
  ], [
    echo "${CONTRIB_DIR}/lib not found" >&AS_MESSAGE_LOG_FD
    CONTRIB_DIR=""
    AC_MSG_RESULT([not usable])
  ])

])
AS_IF([test -n "${CONTRIB_DIR}"], [
  AC_MSG_RESULT([${CONTRIB_DIR}])
  CPPFLAGS="${CPPFLAGS} -I${CONTRIB_DIR}/include"
  CFLAGS="${CFLAGS} -I${CONTRIB_DIR}/include"
  CXXFLAGS="${CXXFLAGS} -I${CONTRIB_DIR}/include"
  OBJCFLAGS="${OBJCFLAGS} -I${CONTRIB_DIR}/include -F${CONTRIB_DIR} -F${CONTRIB_DIR}/Frameworks"
  PKG_CONFIG_PATH="${CONTRIB_DIR}/lib/pkgconfig:${CONTRIB_DIR}/share/pkgconfig:${PKG_CONFIG_PATH}"
  case "${build_os}" in
      mingw32)
          export PKG_CONFIG_PATH=`cygpath -mp ${PKG_CONFIG_PATH}`
      ;;
      *)
          export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}"
      ;;
  esac
  LDFLAGS="${LDFLAGS} -L${CONTRIB_DIR}/lib"

  AS_IF([test -z "$LUAC"], [
    dnl Old contribs mixed cross-tools and cross-compiled executables
    AS_IF([test -x "${CONTRIB_DIR}/bin/luac${BUILDEXEEXT}"], [
      LUAC="${CONTRIB_DIR}/bin/luac${BUILDEXEEXT}"
    ])

    dnl Newer contribs follow usual name space rules
    AS_IF([test -x "${CONTRIB_DIR}/../bin/${host_alias}-luac${BUILDEXEEXT}"], [
      LUAC="${CONTRIB_DIR}/../bin/${host_alias}-luac${BUILDEXEEXT}"
    ])
  ])

  AS_IF([test "${SYS}" = "darwin"], [
    export LD_LIBRARY_PATH="${CONTRIB_DIR}/lib:$LD_LIBRARY_PATH"
    export DYLD_LIBRARY_PATH="${CONTRIB_DIR}/lib:$DYLD_LIBRARY_PATH"
  ])
], [
  AS_IF([test -n "${with_contrib}" && test "${with_contrib}" != "no"], [
    AC_MSG_ERROR([Third party libraries not found!])
  ])
])
AC_SUBST([CONTRIB_DIR])

dnl Add extras/tools to the PATH
TOOLS_DIR="${srcdir}/extras/tools/build/bin"
AS_IF([test -d "${TOOLS_DIR}"], [
    TOOLS_DIR=`cd "${TOOLS_DIR}" && pwd`
    export PATH="${TOOLS_DIR}":$PATH
])

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

LT_INIT([dlopen win32-dll shared disable-static])
LT_LANG([C++])
LT_LANG([Windows Resource])

DOLT

lt_cv_deplibs_check_method=pass_all

AS_IF([test "${enable_shared}" = "no" -a "${enable_vlc}" != "no"], [
  AC_MSG_ERROR([VLC is based on plugins. Shared libraries cannot be disabled.])
])

dnl
dnl Gettext stuff
dnl
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external], [need-formatstring-macros])

dnl
dnl Iconv stuff
dnl
AM_ICONV

dnl
dnl checks for mingw
AS_IF([test "${SYS}" = "mingw32"], [

AC_PREPROC_IFELSE([AC_LANG_SOURCE([
#if !defined(__MINGW32__)
# error Not compiling with mingw
#endif
    ])], [

dnl Check for broken versions of mingw-runtime compatability library
    AC_MSG_CHECKING(for broken mingw-runtime)
    AC_PREPROC_IFELSE([AC_LANG_SOURCE([
#include <_mingw.h>
#if defined(__MINGW64_VERSION_MAJOR)
# if __MINGW64_VERSION_MAJOR < 5
#  error Update your mingw-w64! This one is too old.
# endif
#else
# error You cannot compile with Mingw. Use Mingw-w64!
#endif
    ])], [
        AC_MSG_RESULT([ok])
    ], [
        AC_MSG_RESULT([present])
        AC_MSG_ERROR([LibVLC requires Mingw-w64 version 5.0 or higher!])
    ])

    dnl force use of mingw provided c99 *printf over msvcrt
    CPPFLAGS="${CPPFLAGS} -D__USE_MINGW_ANSI_STDIO=1"

    dnl Check for the need to include the mingwex lib for mingw32
    VLC_SAVE_FLAGS
    AC_CHECK_LIB([mingwex], [opendir],
        AC_CHECK_LIB([mingw32], [opendir],,
            [VLC_ADD_LIBS([libvlccore],[-lmingwex])])
    )
    VLC_RESTORE_FLAGS
    ], [
        AC_MSG_RESULT([Not compiling with mingw])
    ])

    dnl Check for fnative-struct or mms-bitfields support for mingw32
    AX_CHECK_COMPILE_FLAG([-mms-bitfields], [
        AX_APPEND_FLAG([-mms-bitfields], [CFLAGS])
        AX_APPEND_FLAG([-mms-bitfields], [CXXFLAGS])
        # Check for the warning flag without "-Wno-", GCC accepts
        # -Wno-<anything> for unsupported warnings, which can trigger
        # other warnings instead.
        AX_CHECK_COMPILE_FLAG([-Wincompatible-ms-struct], [
            AX_APPEND_FLAG([-Wno-incompatible-ms-struct], [CFLAGS])
            AX_APPEND_FLAG([-Wno-incompatible-ms-struct], [CXXFLAGS])
        ])
    ], [
        AX_CHECK_COMPILE_FLAG([-fnative-struct], [
            AX_APPEND_FLAG([-fnative-struct], [CFLAGS])
            AX_APPEND_FLAG([-fnative-struct], [CXXFLAGS])
        ])
    ])
])


dnl
dnl Buggy glibc prevention. Purposedly not cached.
dnl See sourceware.org bugs 5058 and 5443.
dnl Ubuntu alone has 20 bug numbers for this...
dnl
AC_MSG_CHECKING(for buggy GNU/libc versions)
AC_PREPROC_IFELSE([AC_LANG_SOURCE([
#include <limits.h>
#if defined (__GLIBC__) && (__GLIBC__ == 2) \
  && (__GLIBC_MINOR__ >= 5) && (__GLIBC_MINOR__ <= 7)
# error GNU/libc with dcgettext killer bug!
#endif
])], [
  AC_MSG_RESULT([not present])
], [
  AC_MSG_RESULT([found])
  AS_IF([test "x${enable_nls}" != "xno"], [
    AC_MSG_ERROR([Buggy GNU/libc (version 2.5 - 2.7) present. VLC would crash; there is no viable
work-around for this. Check with your distribution vendor on how to update the
glibc run-time. Alternatively, build with --disable-nls.])
  ])
])

dnl Plugin compilation stuff

VLC_LIBRARY_SUFFIX

dnl Check for system libs needed
need_libc=false

dnl Check for usual libc functions
AC_CHECK_FUNCS([accept4 fcntl flock fstatat fstatvfs fork getmntent_r getenv getpwuid_r isatty memalign mkostemp mmap open_memstream newlocale pipe2 pread posix_fadvise posix_madvise setlocale stricmp strnicmp strptime uselocale])
AC_REPLACE_FUNCS([aligned_alloc atof atoll dirfd fdopendir flockfile fsync getdelim getpid lfind lldiv memrchr nrand48 poll posix_memalign recvmsg rewind sendmsg setenv strcasecmp strcasestr strdup strlcpy strndup strnlen strnstr strsep strtof strtok_r strtoll swab tdestroy tfind timegm timespec_get strverscmp pathconf])
AC_REPLACE_FUNCS([gettimeofday])
AC_CHECK_FUNC(fdatasync,,
  [AC_DEFINE(fdatasync, fsync, [Alias fdatasync() to fsync() if missing.])
])

VLC_REPLACE_DECL([realpath], [#include <stdlib.h>])

dnl mingw64 implements those as static inline, not functions with C linkage
VLC_REPLACE_DECL([asprintf], [#include <stdio.h>])
VLC_REPLACE_DECL([vasprintf], [#include <stdio.h>])
VLC_REPLACE_DECL([gmtime_r], [#include <time.h>])
VLC_REPLACE_DECL([localtime_r], [#include <time.h>])

dnl C11 static_assert()
AC_MSG_CHECKING([for static_assert in assert.h])
AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <assert.h>], [
static_assert(1, "The impossible happened.");
])], [
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_STATIC_ASSERT], [1], [Define to 1 if <assert.h> defines static_assert.])
], [
  AC_MSG_RESULT([no])
])

# Windows CE does not have strcoll()
AC_FUNC_STRCOLL

dnl Check for non-standard system calls
case "$SYS" in
  "linux")
    AC_CHECK_FUNCS([eventfd vmsplice sched_getaffinity recvmmsg memfd_create])
    ;;
  "mingw32")
    AC_CHECK_FUNCS([_lock_file])
    ;;
esac

AH_BOTTOM([#include <vlc_fixups.h>])

dnl Check for struct timespec
AC_CHECK_TYPES([struct timespec],,,
[#include <time.h>])

dnl Check for max_align_t
AC_CHECK_TYPES([max_align_t],,,
[#include <stddef.h>])

dnl Checks for socket stuff
VLC_SAVE_FLAGS
SOCKET_LIBS=""
AC_SEARCH_LIBS(connect, [socket], [
  AS_IF([test "$ac_cv_search_connect" != "none required"], [
    SOCKET_LIBS="$ac_cv_search_connect"
  ])
], [
  AS_IF([test "${SYS}" = "mingw32"], [
    SOCKET_LIBS="-lws2_32 -liphlpapi"
  ])
])

AC_SEARCH_LIBS([inet_pton], [nsl], [
 AS_IF([test "$ac_cv_search_inet_pton" != "none required"], [
    SOCKET_LIBS="$ac_cv_search_inet_pton $SOCKET_LIBS"
  ])
],, [${SOCKET_LIBS}])

LIBS="${LIBS} ${SOCKET_LIBS}"
AC_LINK_IFELSE([
    AC_LANG_PROGRAM([#ifdef _WIN32
        #include <ws2tcpip.h>
        #include <windows.h>
        # if _WIN32_WINNT < 0x600
        #  error Needs vista+
        # endif
        #else
        #include <sys/socket.h>
        #include <arpa/inet.h>
        #endif], [
        char dst[[sizeof(struct in_addr)]];
        inet_pton(AF_INET, "127.0.0.1", dst);
    ])],[AC_DEFINE([HAVE_INET_PTON],[1],[Define to 1 if you have inet_pton function])],[AC_LIBOBJ([inet_pton])])
AC_CHECK_TYPES([struct if_nameindex],,,[#include <net/if.h>])
AC_CHECK_FUNCS([if_nameindex])
VLC_RESTORE_FLAGS
AC_SUBST([SOCKET_LIBS])

dnl Check for socklen_t
AH_TEMPLATE(socklen_t, [Define to `int' if <sys/socket.h> does not define.])
AC_CACHE_CHECK([for socklen_t in sys/socket.h], dnl ` (fix VIM syntax highlight
ac_cv_type_socklen_t,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#include <sys/types.h>
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <sys/socket.h>
#endif]], [[socklen_t len; len = 0;]])],
ac_cv_type_socklen_t=yes,
ac_cv_type_socklen_t=no)])
AS_IF([test "$ac_cv_type_socklen_t" = no], [
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
#include <sys/types.h>
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <sys/socket.h>
#endif
    ]], [[
_Generic(getpeername, int (*)(int, struct sockaddr *, int *): 0);
    ]])
  ], AC_DEFINE(socklen_t, int), AC_DEFINE(socklen_t, unsigned int))
])

dnl Check for struct sockaddr_storage
AH_TEMPLATE(sockaddr_storage, [Define to `sockaddr' if <sys/socket.h> does not define.]) dnl ` (fix VIM syntax highlight
AH_TEMPLATE(ss_family, [Define to `sa_family' if <sys/socket.h> does not define.]) dnl ` (fix VIM syntax highlight
AC_CACHE_CHECK([for struct sockaddr_storage], ac_cv_struct_sockaddr_storage,
  [AC_TRY_COMPILE(
    [#include <sys/types.h>
     #if defined( _WIN32 )
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

dnl FreeBSD has a gnugetopt library for this:
GNUGETOPT_LIBS=""
AC_CHECK_FUNC(getopt_long,, [
  AC_CHECK_LIB([gnugetopt],[getopt_long], [
    GNUGETOPT_LIBS="-lgnugetopt"
  ])
])
AC_SUBST([GNUGETOPT_LIBS])

AC_CHECK_LIB([m], [cos], [
  LIBM="-lm"
], [
  LIBM=""
])
AC_SUBST([LIBM])

AC_CHECK_LIB([m], [lrintf], [
  AC_DEFINE(HAVE_LRINTF, 1, [Define to 1 if you have the lrintf function])
  VLC_ADD_LIBS([skins2],[-lm])
])
AC_CHECK_LIB([m], [nanf],
  AC_DEFINE(HAVE_NANF, 1, [Define to 1 if you have the NANF function])
)
AC_CHECK_LIB([m], [sincos], [
  AC_DEFINE(HAVE_SINCOS, 1, [Define to 1 if you have the sincos function.])
], [
  AC_LIBOBJ([sincos])
])

AC_MSG_CHECKING([broken qsort_r prototype])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([#include <stdlib.h>

_Static_assert(_Generic((qsort_r),
    void (*)(void *, size_t, size_t, void *,
             int (*)(void *, const void *, const void *)) : 1, default: 0),
    "Bad prototype not matched");
])], [
  AC_MSG_RESULT([broken])
  AC_DEFINE([HAVE_BROKEN_QSORT_R], 1, [Defined to 1 if the qsort_r() prototype contradicts the upcoming POSIX standard.])
], [
  AC_MSG_RESULT([no])
])

AC_MSG_CHECKING([_Thread_local support])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([_Thread_local int foo = 0;
                   int main() {}])], [
    AC_MSG_RESULT([ok])
    AC_DEFINE(HAVE_THREAD_LOCAL, 1, [Defined to 1 if C11 _Thread_local storage qualifier is supported])
],[ AC_MSG_RESULT([no])])

dnl __STDC_NO_THREADS__ can't be trusted on some platforms.
dnl check for its availability explicitely
AC_CHECK_HEADERS([threads.h])

AC_CHECK_LIB([anl],[getaddrinfo_a],[
  AC_DEFINE(HAVE_LIBANL, 1, [Define to 1 if you have the anl library])
])

dnl Check for dynamic plugins
LIBDL=""
have_dynamic_objects="no"
VLC_SAVE_FLAGS
AC_SEARCH_LIBS(dlsym, [dl svld], [
  AS_IF([test "$ac_cv_search_dlsym" != "none required"], [
    LIBDL="$ac_cv_search_dlsym"
  ])
  have_dynamic_objects="yes"
])
VLC_RESTORE_FLAGS

# Windows
AS_IF([test "${SYS}" = "mingw32"], [
  LIBDL=""
  have_dynamic_objects="yes" #assume we can use shared objects
])

AC_ARG_WITH([libfuzzer],
    AS_HELP_STRING([--with-libfuzzer=DIR],
        [use libFuzzer library from DIR and triggers sanitize coverage (default disabled)]),
     [],
     [with_libfuzzer=no])
AS_VAR_IF(with_libfuzzer, no, [], [
    enable_shared=no
    enable_static=yes
    enable_vlc=no
])
AM_CONDITIONAL([HAVE_LIBFUZZER], [test "${with_libfuzzer}" != "no"])

AS_IF([test "${enable_shared}" = "no"], [
  have_dynamic_objects=no
])
AM_CONDITIONAL([HAVE_DYNAMIC_PLUGINS], [test "${have_dynamic_objects}" != "no"])

AC_SUBST([LIBDL])

AS_IF([test "${SYS}" != "mingw32"], [
  AC_SEARCH_LIBS([clock_nanosleep], [rt pthread], [
    AS_IF([test "$ac_cv_search_clock_nanosleep" != "none required"], [
      LIBRT="$ac_cv_search_clock_nanosleep"
    ])
  ], [
    AC_SEARCH_LIBS([nanosleep], [rt pthread posix4], [
      AS_IF([test "$ac_cv_search_nanosleep" != "none required"], [
        LIBRT="$ac_cv_search_nanosleep"
      ])
    ])
  ])
])
AC_SUBST([LIBRT])

dnl  Check for clock_* functions, needs to be done here,
dnl  after the -lrt check
AC_REPLACE_FUNCS([clock_gettime clock_nanosleep clock_getres])

dnl
dnl Check for headers
dnl

dnl  POSIX
AC_CHECK_HEADERS([arpa/inet.h poll.h pthread.h search.h sys/shm.h sys/socket.h sys/uio.h])
AC_CHECK_HEADERS([net/if.h], [], [],
[
#include <sys/types.h>
#include <sys/socket.h>
])
AC_CHECK_HEADER([syslog.h], [have_syslog="yes"], [have_syslog="no"])
AM_CONDITIONAL([HAVE_SYSLOG], [test "$have_syslog" = "yes"])

dnl  BSD
AC_CHECK_HEADERS([netinet/tcp.h netinet/udplite.h sys/param.h sys/mount.h])

dnl  GNU/Linux
AC_CHECK_HEADERS([features.h getopt.h linux/dccp.h linux/magic.h sys/eventfd.h])

dnl  MacOS
AC_CHECK_HEADERS([xlocale.h])

dnl Check for struct pollfd
AC_CHECK_TYPES([struct pollfd],,,
[#include <sys/types.h>
#if HAVE_POLL_H
# include <poll.h>
#elif defined (_WIN32)
# include <winsock2.h>
#endif
])

dnl Check if C++ headers define locale_t
AC_LANG_PUSH(C++)
AC_CACHE_CHECK([if C++ headers define locale_t],
  [ac_cv_cxx_locale_t],
  [ac_cv_cxx_locale_t=no
   AC_TRY_COMPILE([#include <locale>], [locale_t loc;],
                    [ac_cv_cxx_locale_t=yes])])
if test "${ac_cv_cxx_locale_t}" != "no"; then
  AC_DEFINE(HAVE_CXX_LOCALE_T, 1, Define to 1 if C++ headers define locale_t)
fi
AC_LANG_POP(C++)

dnl LP64 and LLP64 architectures had better define ssize_t by themselves...
AH_TEMPLATE(ssize_t, [Define to `int' if <stddef.h> does not define.]) dnl ` (fix VIM syntax highlight
AC_CHECK_TYPE(ssize_t,, [
  AC_DEFINE(ssize_t, int)
])

dnl It seems that autoconf detects pkg-config only during the first
dnl PKG_CHECK_MODULES from configure.ac - which makes sense. But in our case,
dnl it is nested within a conditional block, so it was not working right.
PKG_PROG_PKG_CONFIG()

dnl On some OS we need static linking
AS_IF([test -n "${PKG_CONFIG}" ],[
    AS_IF([test "${SYS}" = "mingw32" -o "${SYS}" = "darwin" -o "${SYS}" = "os2" ],[
        PKG_CONFIG="${PKG_CONFIG} --static"
    ])
])


dnl
dnl Check for zlib.h and -lz if available
dnl
AC_CHECK_HEADERS([zlib.h], [ have_zlib=yes ], [ have_zlib=no ])
AM_CONDITIONAL([HAVE_ZLIB], [ test "${have_zlib}" = "yes" ])
if test "${have_zlib}" = "yes"
then
  VLC_ADD_LIBS([sap],[-lz])
fi


dnl
dnl Domain name i18n support via GNU libidn
dnl
PKG_CHECK_MODULES([IDN], [libidn], [
  have_libidn="yes"
  AC_DEFINE([HAVE_IDN], 1, [Define to 1 if you have GNU libidn.])
], [
  have_libidn="no"
])


dnl Check for dbus
AC_ARG_ENABLE([dbus],
  AS_HELP_STRING([--enable-dbus],
    [compile D-Bus message bus support (default enabled)]))
have_dbus="no"
AS_IF([test "${enable_dbus}" != "no"], [
  PKG_CHECK_MODULES(DBUS, [dbus-1 >= 1.6.0], [
    have_dbus="yes"
  ], [
    AS_IF([test -n "${enable_dbus}"], [
      AC_MSG_ERROR([${DBUS_PKG_ERRORS}.])
    ], [
      AC_MSG_WARN([${DBUS_PKG_ERRORS}.])
    ])
  ])
])
AM_CONDITIONAL([HAVE_DBUS], [test "${have_dbus}" = "yes"])


dnl Check for systemd
PKG_CHECK_MODULES([SYSTEMD], [libsystemd], [
  have_systemd="yes"
], [
  AC_MSG_WARN([${SYSTEMD_PKG_ERRORS}.])
])
AM_CONDITIONAL([HAVE_SYSTEMD], [test "${have_systemd}" = "yes"])


EXTEND_HELP_STRING([Optimization options:])
dnl
dnl  Compiler warnings
dnl

AX_APPEND_COMPILE_FLAGS([-Wall -Wextra -Wsign-compare -Wundef -Wpointer-arith -Wvolatile-register-var -Wformat -Wformat-security -Wduplicated-branches -Wduplicated-cond], [CFLAGS])
AX_APPEND_COMPILE_FLAGS([-Wall -Wextra -Wsign-compare -Wundef -Wpointer-arith -Wvolatile-register-var -Wformat -Wformat-security -Wduplicated-branches -Wduplicated-cond], [CXXFLAGS])

dnl -Werror-implicit-function-declaration is used for the if_nametoindex detection with winstore
AX_APPEND_COMPILE_FLAGS([-Wbad-function-cast -Wwrite-strings -Wmissing-prototypes -Werror-implicit-function-declaration -Winit-self -Wlogical-op -Wshadow=local -Wmultistatement-macros], [CFLAGS])
AX_APPEND_COMPILE_FLAGS([-pipe], [CFLAGS])

dnl UWP forbidden checks
AS_IF([test "${SYS}" = "mingw32"], [
  AC_MSG_CHECKING(for if_nametoindex availability)
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
      [[#include <windows.h>
        #if defined(__MINGW64_VERSION_MAJOR) && __MINGW64_VERSION_MAJOR < 6
        # include <wincrypt.h>
        #endif
        #include <iphlpapi.h>
        static void test (void) {
          if_nametoindex("");
        }
      ]],[[;]])
    ],[
      AC_MSG_RESULT([present])
      AC_DEFINE([HAVE_IF_NAMETOINDEX], [1], [Define to 1 if you have if_nametoindex function])
    ],[
      AC_MSG_RESULT([not present])
    ])
])

dnl
dnl  Debugging mode
dnl
AC_ARG_ENABLE([debug],
  AS_HELP_STRING([--disable-debug],
    [disable run-time assertions (default enabled)]),,
  [enable_debug="yes"])
AH_TEMPLATE(NDEBUG,
            [Define to 1 if debug code should NOT be compiled])
AS_IF([test "${enable_debug}" != "no"], [
  AC_CHECK_HEADERS([valgrind/valgrind.h])
], [
  AC_DEFINE(NDEBUG)
])

dnl
dnl  Profiling
dnl
AC_ARG_ENABLE([gprof],
  AS_HELP_STRING([--enable-gprof],[profile with gprof (default disabled)]),,
  [enable_gprof="no"])
AS_IF([test "${enable_gprof}" != "no"], [
  CFLAGS="${CFLAGS} -pg"
  CXXFLAGS="${CXXFLAGS} -pg"
  OBJCFLAGS="${OBJCFLAGS} -pg"
  LDFLAGS="${LDFLAGS} -pg"
])

AC_ARG_ENABLE([cprof],
  AS_HELP_STRING([--enable-cprof],[profile with cprof (default disabled)]),,
  [enable_cprof="no"])
AS_IF([test "${enable_gprof}" != "no"], [
  CFLAGS="${CFLAGS} -finstrument-functions"
  CXXFLAGS="${CXXFLAGS} -finstrument-functions"
  OBJCFLAGS="${OBJCFLAGS} -finstrument-functions"
  LDFLAGS="${LDFLAGS} -finstrument-functions"
])

dnl
dnl  Test coverage
dnl
AC_ARG_ENABLE([coverage],
  AS_HELP_STRING([--enable-coverage],
    [build for test coverage (default disabled)]),,
  [enable_coverage="no"])
AS_IF([test "${enable_coverage}" != "no"], [
  CFLAGS="-fprofile-arcs -ftest-coverage ${CFLAGS}"
  CXXFLAGS="-fprofile-arcs -ftest-coverage ${CXXFLAGS}"
  LDFLAGS="-lgcov ${LDFLAGS}"
])

AS_IF([test "${SYS}" != "mingw32" -a "${SYS}" != "os2"], [
  VLC_SAVE_FLAGS
  CFLAGS="${CFLAGS} -fvisibility=hidden"
  CXXFLAGS="${CXXFLAGS} -fvisibility=hidden"
  OBJCFLAGS="${OBJCFLAGS} -fvisibility=hidden"
  AC_CACHE_CHECK([if $CC accepts -fvisibility=hidden],
                 [ac_cv_c_visibility_hidden], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
      ac_cv_c_visibility_hidden=yes
    ], [
      ac_cv_c_visibility_hidden=no
    ])
  ])
  AS_IF([test "${ac_cv_c_visibility_hidden}" = "no"], [VLC_RESTORE_FLAGS])
])

dnl
dnl Sanitizer flags
dnl
AC_ARG_WITH([sanitizer],
    AS_HELP_STRING([--with-sanitizer=(address/memory/undefined/thread)],
        [build with sanitizer flags (default disabled)]),
     [],
     [with_sanitizer=no])

AS_VAR_IF(with_sanitizer, no, [], [
    AX_CHECK_COMPILE_FLAG([-fsanitize=${with_sanitizer}], [
        AX_APPEND_FLAG([-fsanitize=${with_sanitizer}])
        AX_APPEND_FLAG([-fsanitize=${with_sanitizer}], [CXXFLAGS])
        AX_APPEND_FLAG([-fsanitize=${with_sanitizer}], [LDFLAGS])
    ], [
        AC_MSG_ERROR(["-fsanitize=${with_sanitizer} not supported!"])
    ])
    AX_APPEND_FLAG([-g])
    AX_APPEND_FLAG([-g], [CXXFLAGS])

    AS_IF([test -z "${with_sanitizer##*address*}" ], [
       AX_APPEND_COMPILE_FLAGS([-fsanitize-address-use-after-scope -fno-omit-frame-pointer -fsanitize=pointer-compare -fsanitize=pointer-subtract])
       AX_APPEND_COMPILE_FLAGS([-fsanitize-address-use-after-scope -fno-omit-frame-pointer -fsanitize=pointer-compare -fsanitize=pointer-subtract], [CXXFLAGS])
    ])
    AS_IF([test -z "${with_sanitizer##*memory*}" ], [
        AX_APPEND_COMPILE_FLAGS([-fPIE -pie])
        AX_APPEND_COMPILE_FLAGS([-fPIE -pie], [CXXFLAGS])
    ])
    AS_IF([test -z "${with_sanitizer##*thread*}" ], [
        AX_APPEND_COMPILE_FLAGS([-fPIE -pie])
        AX_APPEND_COMPILE_FLAGS([-fPIE -pie], [CXXFLAGS])
    ])
])

dnl
dnl  Enable/disable optimizations
dnl
AC_ARG_ENABLE([optimizations],
  AS_HELP_STRING([--disable-optimizations],
    [disable compiler optimizations (default enabled)]),,
  [enable_optimizations="yes"])

dnl Check for various optimization flags
AS_IF([test "${enable_optimizations}" != "no"], [

  dnl -O3 only in production builds
  AS_IF([test "${enable_debug}" = "no"], [
    VLC_SAVE_FLAGS
    CFLAGS="${CFLAGS} -O3"
    CXXFLAGS="${CXXFLAGS} -O3"
    OBJCFLAGS="${OBJCFLAGS} -O3"
    AC_CACHE_CHECK([if $CC accepts -O3],  [ac_cv_c_o3], [
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
        ac_cv_c_o3=yes
      ], [
        ac_cv_c_o3=no
      ])
    ])
    AS_IF([test "${ac_cv_c_o3}" = "no"], [VLC_RESTORE_FLAGS])
  ])

  dnl Check for fast maths
  AX_APPEND_COMPILE_FLAGS([-fno-math-errno -funsafe-math-optimizations -fno-rounding-math -fno-signaling-nans -fcx-limited-range], [CFLAGS])
  AX_APPEND_COMPILE_FLAGS([-fno-math-errno -funsafe-math-optimizations -fno-rounding-math -fno-signaling-nans -fcx-limited-range], [CXXFLAGS])

AH_BOTTOM([
#if defined(_MSC_VER) && !defined(__clang__)
# pragma fenv_access(off)
# pragma fp_contract(on)
#elif defined(__GNUC__)
/* Not supported so far */
#else
# pragma STDC FENV_ACCESS OFF
# pragma STDC FP_CONTRACT ON
#endif
])

  dnl Check for -funroll-loops
  VLC_SAVE_FLAGS
  CFLAGS="${CFLAGS} -funroll-loops"
  CXXFLAGS="${CXXFLAGS} -funroll-loops"
  OBJCFLAGS="${OBJCFLAGS} -funroll-loops"
  AC_CACHE_CHECK([if $CC accepts -funroll-loops], [ac_cv_c_unroll_loops], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
      ac_cv_c_unroll_loops=yes
    ], [
      ac_cv_c_unroll_loops=no
    ])
  ])
  AS_IF([test "${ac_cv_c_unroll_loops}" = "no"], [VLC_RESTORE_FLAGS])

  AS_IF([test "$enable_debug" = "no"], [
    dnl Check for -fomit-frame-pointer
    VLC_SAVE_FLAGS
    CFLAGS="${CFLAGS} -fomit-frame-pointer"
    CXXFLAGS="${CXXFLAGS} -fomit-frame-pointer"
    OBJCFLAGS="${OBJCFLAGS} -fomit-frame-pointer"
    AC_CACHE_CHECK([if $CC accepts -fomit-frame-pointer],
      [ac_cv_c_omit_frame_pointer], [
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
        ac_cv_c_omit_frame_pointer=yes
      ], [
        ac_cv_c_omit_frame_pointer=no
      ])
    ])
    AS_IF([test "${ac_cv_c_omit_frame_pointer}" = "no"], [VLC_RESTORE_FLAGS])
  ])

])

AC_ARG_ENABLE([ssp],
  AS_HELP_STRING([--disable-ssp],
  [disable stack smashing protection (default enabled for optimized builds)]), [],
  [enable_ssp="yes"])

AS_IF([test "${enable_ssp}" != "no" -a "${enable_optimizations}" != "no"], [
  VLC_SAVE_FLAGS
  AX_APPEND_COMPILE_FLAGS([-fstack-protector-strong])
  AX_APPEND_COMPILE_FLAGS([-fstack-protector-strong], [CXXFLAGS])
  dnl Win32 requires linking to ssp for stack-protection
  AS_IF([test "${SYS}" = "mingw32"], [
      LDFLAGS="${LDFLAGS} -lssp"
      AC_CACHE_CHECK([if linker supports stack protectors], [ac_cv_ld_ssp], [
        AC_TRY_LINK([#include <stdio.h>],[char buf[100]; fgets(buf, sizeof(buf), stdin);], [
          ac_cv_ld_ssp="yes"
        ], [
          ac_cv_ld_ssp="no"
        ])
      ])
      AS_IF([test "${ac_cv_ld_ssp}" = "no"], [VLC_RESTORE_FLAGS])
  ])
])

AS_IF([test "${SYS}" = "mingw32"], [
    dnl library for BCrypt APIs
    AS_IF([test "${vlc_winstore_app}" = 1],
          VLC_ADD_LIBS([libvlccore], [-lbcrypt]))
])

VLC_SAVE_FLAGS
LDFLAGS="${LDFLAGS} -Wl,-Bsymbolic"
AC_CACHE_CHECK([if linker supports -Bsymbolic], [ac_cv_ld_bsymbolic], [
  AC_TRY_LINK([],, [
    ac_cv_ld_bsymbolic="-Wl,-Bsymbolic"
  ], [
    ac_cv_ld_bsymbolic=""
  ])
])
VLC_RESTORE_FLAGS
SYMBOLIC_LDFLAGS="${ac_cv_ld_bsymbolic}"
AC_SUBST([SYMBOLIC_LDFLAGS])

VLC_SAVE_FLAGS
LDFLAGS="${LDFLAGS} -Wl,-z,defs"
AC_CACHE_CHECK([if linker supports -z,defs], [ac_cv_ld_z_defs], [
  AC_TRY_LINK([],, [
    ac_cv_ld_z_defs="yes"
  ], [
    ac_cv_ld_z_defs="no"
  ])
])
AS_IF([test "${ac_cv_ld_z_defs}" = "no"], [VLC_RESTORE_FLAGS])

dnl Check for __attribute__((packed))
AC_CACHE_CHECK([for __attribute__((packed))],
  [ac_cv_c_attribute_packed],
  [ac_cv_c_attribute_packed=no
   AC_TRY_COMPILE(, [struct __attribute__((__packed__)) foo { int a; } b; (void)b;],
                    [ac_cv_c_attribute_packed=yes])])
if test "${ac_cv_c_attribute_packed}" != "no"; then
  AC_DEFINE(HAVE_ATTRIBUTE_PACKED, 1, Support for __attribute__((packed)) for structs)
fi

dnl
dnl  Check the CPU
dnl
case "${host_cpu}" in
  "")
    ARCH=unknown
    ;;
  armv*)
    ARCH="arm"
    host_cpu="arm"
    ;;
  *)
    ARCH="${host_cpu}"
    ;;
esac

dnl Check for backtrace() support
AC_CHECK_HEADERS([execinfo.h])
AC_CHECK_FUNCS([backtrace])

dnl
dnl  default modules
dnl
ALIASES="${ALIASES} cvlc rvlc"

dnl
dnl  Accelerated modules
dnl

dnl  Check for fully working MMX intrinsics
dnl  We need support for -mmmx, we need <mmintrin.h>, and we also need a
dnl  working compiler (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=23963)
AC_ARG_ENABLE([mmx],
  AS_HELP_STRING([--disable-mmx],
    [disable MMX optimizations (default auto)]),,[
  case "${host_cpu}" in
    i?86|x86_64)
      enable_mmx="yes"
      ;;
    *)
      enable_mmx="no"
      ;;
  esac
])
have_mmx="no"
have_mmxext="no"
AS_IF([test "${enable_mmx}" != "no"], [
  ARCH="${ARCH} mmx"
  VLC_SAVE_FLAGS
  CFLAGS="${CFLAGS} -mmmx"
  AC_CACHE_CHECK([if $CC groks MMX intrinsics], [ac_cv_c_mmx_intrinsics], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#include <mmintrin.h>
#include <stdint.h>
uint64_t frobzor;]], [
[__m64 a, b, c;
a = b = c = (__m64)frobzor;
a = _mm_slli_pi16(a, 3);
a = _mm_adds_pi16(a, b);
c = _mm_srli_pi16(c, 8);
c = _mm_slli_pi16(c, 3);
b = _mm_adds_pi16(b, c);
a = _mm_unpacklo_pi8(a, b);
frobzor = (uint64_t)a;]])], [
      ac_cv_c_mmx_intrinsics=yes
    ], [
      ac_cv_c_mmx_intrinsics=no
    ])
  ])
  VLC_RESTORE_FLAGS
  AS_IF([test "${ac_cv_c_mmx_intrinsics}" != "no"], [
    AC_DEFINE(HAVE_MMX_INTRINSICS, 1, [Define to 1 if MMX intrinsics are available.])
  ])

  VLC_SAVE_FLAGS
  CFLAGS="${CFLAGS} -mmmx"
  AC_CACHE_CHECK([if $CC groks MMX inline assembly], [ac_cv_mmx_inline], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("packuswb %%mm1,%%mm2"::"r"(p):"mm1", "mm2");
]])
    ], [
      ac_cv_mmx_inline=yes
    ], [
      ac_cv_mmx_inline=no
    ])
  ])
  AS_IF([test "${ac_cv_mmx_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_MMX, 1, [Define to 1 inline MMX assembly is available.])
    have_mmx="yes"
  ])

  AC_CACHE_CHECK([if $CC groks MMX EXT inline assembly],
                 [ac_cv_mmxext_inline], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("maskmovq %%mm1,%%mm2"::"r"(p):"mm1", "mm2");
]])
    ], [
      ac_cv_mmxext_inline=yes
    ], [
      ac_cv_mmxext_inline=no
    ])
  ])
  VLC_RESTORE_FLAGS
  AS_IF([test "${ac_cv_mmxext_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_MMXEXT, 1, [Define to 1 if MMX EXT inline assembly is available.])
    have_mmxext="yes"
  ])
])
AM_CONDITIONAL([HAVE_MMX], [test "${have_mmx}" = "yes"])
AM_CONDITIONAL([HAVE_MMXEXT], [test "${have_mmxext}" = "yes"])

dnl  Check for fully workin SSE2 intrinsics
dnl  We need support for -msse[2], we need <emmintrin.h>, and we also need a
dnl  working compiler (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=23963)
AC_ARG_ENABLE([sse],
  AS_HELP_STRING([--disable-sse],
    [disable SSE (1-4) optimizations (default auto)]),, [
  case "${host_cpu}" in
    i?86|x86_64)
      enable_sse=yes
      ;;
    *)
      enable_sse=no
      ;;
  esac
])
have_sse2="no"
AS_IF([test "${enable_sse}" != "no"], [
  ARCH="${ARCH} sse sse2"

  VLC_SAVE_FLAGS
  CFLAGS="${CFLAGS} -msse2"
  AC_CACHE_CHECK([if $CC groks SSE2 intrinsics], [ac_cv_c_sse2_intrinsics], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#include <emmintrin.h>
#include <stdint.h>
uint64_t frobzor;]], [
[__m128i a, b, c;
a = b = c = _mm_set1_epi64((__m64)frobzor);
a = _mm_slli_epi16(a, 3);
a = _mm_adds_epi16(a, b);
c = _mm_srli_epi16(c, 8);
c = _mm_slli_epi16(c, 3);
b = _mm_adds_epi16(b, c);
a = _mm_unpacklo_epi8(a, b);
frobzor = (uint64_t)_mm_movepi64_pi64(a);]])], [
      ac_cv_c_sse2_intrinsics=yes
    ], [
      ac_cv_c_sse2_intrinsics=no
    ])
  ])
  VLC_RESTORE_FLAGS
  AS_IF([test "${ac_cv_c_sse2_intrinsics}" != "no"], [
    AC_DEFINE(HAVE_SSE2_INTRINSICS, 1, [Define to 1 if SSE2 intrinsics are available.])
  ])

  VLC_SAVE_FLAGS
  CFLAGS="${CFLAGS} -msse"
  AC_CACHE_CHECK([if $CC groks SSE inline assembly], [ac_cv_sse_inline], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("xorps %%xmm1,%%xmm2"::"r"(p):"xmm1", "xmm2");
]])
    ], [
      ac_cv_sse_inline=yes
    ], [
      ac_cv_sse_inline=no
    ])
  ])

  AS_IF([test "${ac_cv_sse_inline}" != "no" -a "${SYS}" != "solaris"], [
    AC_DEFINE(CAN_COMPILE_SSE, 1, [Define to 1 if SSE inline assembly is available.])
  ])

  AC_CACHE_CHECK([if $CC groks SSE2 inline assembly], [ac_cv_sse2_inline], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("punpckhqdq %%xmm1,%%xmm2"::"r"(p):"xmm1", "xmm2");
]])
    ], [
      ac_cv_sse2_inline=yes
    ], [
      ac_cv_sse2_inline=no
    ])
  ])
  AS_IF([test "${ac_cv_sse2_inline}" != "no" -a "${SYS}" != "solaris"], [
    AC_DEFINE(CAN_COMPILE_SSE2, 1, [Define to 1 if SSE2 inline assembly is available.])
    have_sse2="yes"
  ])

  # SSE3
  AC_CACHE_CHECK([if $CC groks SSE3 inline assembly], [ac_cv_sse3_inline], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("movsldup %%xmm1,%%xmm0"::"r"(p):"xmm0", "xmm1");
]])
    ], [
      ac_cv_sse3_inline=yes
    ], [
      ac_cv_sse3_inline=no
    ])
  ])

  AS_IF([test "${ac_cv_sse3_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_SSE3, 1, [Define to 1 if SSE3 inline assembly is available.]) ])

  # SSSE3
  AC_CACHE_CHECK([if $CC groks SSSE3 inline assembly], [ac_cv_ssse3_inline], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("pabsw %%xmm0,%%xmm0"::"r"(p):"xmm0");
]])
    ], [
      ac_cv_ssse3_inline=yes
    ], [
      ac_cv_ssse3_inline=no
    ])
  ])

  AS_IF([test "${ac_cv_ssse3_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_SSSE3, 1, [Define to 1 if SSSE3 inline assembly is available.]) ])

  # SSE4.1
  AC_CACHE_CHECK([if $CC groks SSE4.1 inline assembly],
                 [ac_cv_sse4_1_inline], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("pmaxsb %%xmm1,%%xmm0"::"r"(p):"xmm0", "xmm1");
]])
    ], [
      ac_cv_sse4_1_inline=yes
    ], [
      ac_cv_sse4_1_inline=no
    ])
  ])

  AS_IF([test "${ac_cv_sse4_1_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_SSE4_1, 1, [Define to 1 if SSE4_1 inline assembly is available.]) ])

  # SSE4.2
  AC_CACHE_CHECK([if $CC groks SSE4.2 inline assembly],
                 [ac_cv_sse4_2_inline], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("pcmpgtq %%xmm1,%%xmm0"::"r"(p):"xmm0", "xmm1");
]])
    ], [
      ac_cv_sse4_2_inline=yes
    ], [
      ac_cv_sse4_2_inline=no
    ])
  ])

  AS_IF([test "${ac_cv_sse4_2_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_SSE4_2, 1, [Define to 1 if SSE4_2 inline assembly is available.]) ])

  # SSE4A
  AC_CACHE_CHECK([if $CC groks SSE4A inline assembly], [ac_cv_sse4a_inline], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("insertq %%xmm1,%%xmm0"::"r"(p):"xmm0", "xmm1");
]])
    ], [
      ac_cv_sse4a_inline=yes
    ], [
      ac_cv_sse4a_inline=no
    ])
  ])
  VLC_RESTORE_FLAGS
  AS_IF([test "${ac_cv_sse4a_inline}" != "no"], [
    AC_DEFINE([CAN_COMPILE_SSE4A], [1], [Define to 1 if SSE4A inline assembly is available.]) ])
])
AM_CONDITIONAL([HAVE_SSE2], [test "$have_sse2" = "yes"])

dnl  Check for fully workin AVX2 intrinsics
dnl  We need support for -mavx[2], we need <immintrin.h>, and we also need a
dnl  working compiler (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=23963)
AC_ARG_ENABLE([avx],
  AS_HELP_STRING([--disable-avx],
    [disable AVX (1-2) optimizations (default auto)]),, [
  case "${host_cpu}" in
    i?86|x86_64)
      enable_avx=yes
      ;;
    *)
      enable_avx=no
      ;;
  esac
])
have_avx2="no"
AS_IF([test "${enable_avx}" != "no"], [
  ARCH="${ARCH} avx avx2"

  VLC_SAVE_FLAGS
  CFLAGS="${CFLAGS} -mavx2"
  AC_CACHE_CHECK([if $CC groks AVX2 intrinsics], [ac_cv_c_avx2_intrinsics], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#include <immintrin.h>
#include <stdint.h>
uint64_t frobzor;]], [
[__m256i a, b, c;
a = b = c = _mm256_set1_epi64x((int64_t)frobzor);
a = _mm256_slli_epi16(a, 3);
a = _mm256_adds_epi16(a, b);
c = _mm256_srli_epi16(c, 8);
c = _mm256_slli_epi16(c, 3);
b = _mm256_adds_epi16(b, c);
a = _mm256_unpacklo_epi8(a, b);
frobzor = (uint64_t)_mm256_extract_epi64(a, 0);]])], [
      ac_cv_c_avx2_intrinsics=yes
    ], [
      ac_cv_c_avx2_intrinsics=no
    ])
  ])
  VLC_RESTORE_FLAGS
  AS_IF([test "${ac_cv_c_avx2_intrinsics}" != "no"], [
    AC_DEFINE(HAVE_AVX2_INTRINSICS, 1, [Define to 1 if AVX2 intrinsics are available.])
  ])

  VLC_SAVE_FLAGS
  CFLAGS="${CFLAGS} -mavx"
  AC_CACHE_CHECK([if $CC groks AVX inline assembly], [ac_cv_avx_inline], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("xorps256 %%ymm1,%%ymm2"::"r"(p):"ymm1", "ymm2");
]])
    ], [
      ac_cv_avx_inline=yes
    ], [
      ac_cv_avx_inline=no
    ])
  ])

  AS_IF([test "${ac_cv_avx_inline}" != "no" -a "${SYS}" != "solaris"], [
    AC_DEFINE(CAN_COMPILE_AVX, 1, [Define to 1 if AVX inline assembly is available.])
  ])

  AC_CACHE_CHECK([if $CC groks AVX2 inline assembly], [ac_cv_avx2_inline], [
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("punpckhqdq256 %%ymm1,%%ymm2"::"r"(p):"ymm1", "ymm2");
]])
    ], [
      ac_cv_avx2_inline=yes
    ], [
      ac_cv_avx2_inline=no
    ])
  ])
  VLC_RESTORE_FLAGS
  AS_IF([test "${ac_cv_avx2_inline}" != "no" -a "${SYS}" != "solaris"], [
    AC_DEFINE(CAN_COMPILE_AVX2, 1, [Define to 1 if AVX2 inline assembly is available.])
    have_avx2="yes"
  ])
])
AM_CONDITIONAL([HAVE_AVX2], [test "$have_avx2" = "yes"])

VLC_SAVE_FLAGS
CFLAGS="${CFLAGS} -mmmx"
have_3dnow="no"
AC_CACHE_CHECK([if $CC groks 3D Now! inline assembly], [ac_cv_3dnow_inline], [
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM(,[[
void *p;
asm volatile("pfadd %%mm1,%%mm2"::"r"(p):"mm1", "mm2");
]])
  ], [
    ac_cv_3dnow_inline=yes
  ], [
    ac_cv_3dnow_inline=no
  ])
])
VLC_RESTORE_FLAGS
AS_IF([test "${ac_cv_3dnow_inline}" != "no"], [
  AC_DEFINE([CAN_COMPILE_3DNOW], 1,
            [Define to 1 if 3D Now! inline assembly is available.])
  have_3dnow="yes"
])
AM_CONDITIONAL([HAVE_3DNOW], [test "$have_3dnow" = "yes"])


AC_ARG_ENABLE([neon],
  AS_HELP_STRING([--disable-neon],
    [disable ARM NEON optimizations (default auto)]),, [
  case "${host_cpu}" in
    "arm")
      enable_neon="32"
      ;;
    "aarch64")
      enable_neon="64"
      ;;
    *)
      enable_neon="no"
      ;;
  esac
])
AS_IF([test "${enable_neon}" != "no"], [
  VLC_SAVE_FLAGS
  AC_CACHE_CHECK([if $CCAS groks ARM NEON assembly], [ac_cv_arm_neon], [
    AS_IF([test "${enable_neon}" != "64"], [
      CFLAGS="${CFLAGS} -mfpu=neon -mhard-float"
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM(,[[
asm volatile("vqmovun.s64 d0, q1":::"d0");
]])
      ], [
        ac_cv_arm_neon="32"
      ], [
        ac_cv_arm_neon="no"
      ])
    ], [
      CFLAGS="${CFLAGS} -march=armv8-a+simd"
      AC_COMPILE_IFELSE([
        AC_LANG_PROGRAM(,[[
asm volatile("uhadd v0.8b, v0.8b, v1.8b":::"v0");
]])
      ], [
        ac_cv_arm_neon="64"
      ], [
        ac_cv_arm_neon="no"
      ])
    ])
  ])
  VLC_RESTORE_FLAGS
])
AM_CONDITIONAL([HAVE_NEON], [test "${ac_cv_arm_neon}" = "32"])
AM_CONDITIONAL([HAVE_ARM64], [test "${ac_cv_arm_neon}" = "64"])

AC_ARG_ENABLE([sve],
  AS_HELP_STRING([--disable-sve],
    [disable ARM SVE optimizations (default auto)]),, [
  AS_IF([test "${host_cpu}" = "aarch64"], [enable_sve="yes"] ,[enable_sve="no"])
])
AS_IF([test "${enable_sve}" != "no"], [
  VLC_SAVE_FLAGS
  CFLAGS="${CFLAGS} -march=armv8-a+sve"
  AC_CACHE_CHECK([if $CCAS groks ARM SVE assembly], [ac_cv_arm_sve], [
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM(,[[
asm volatile("ptrue p0.s" ::: "p0");
]])
    ], [
      ac_cv_arm_sve="yes"
    ], [
      ac_cv_arm_sve="no"
    ])
  ])
  VLC_RESTORE_FLAGS
])
AM_CONDITIONAL([HAVE_SVE], [test "${ac_cv_arm_sve}" = "yes"])

AC_ARG_ENABLE([branch-protection],
  AS_HELP_STRING([--disable-branch-protection],
    [disable AArch64 branch protection (default auto)]),, [
  AS_IF([test "${host_cpu}" = "aarch64"],
    [enable_branch_protection="yes"], [enable_branch_protection="no"])
])
AS_IF([test "${enable_branch_protection}" != "no"], [
  AX_CHECK_COMPILE_FLAG([-mbranch-protection=standard], [
    AX_APPEND_FLAG([-mbranch-protection=standard], [CFLAGS])
    AX_APPEND_FLAG([-mbranch-protection=standard], [CXXFLAGS])
  ])
])


AC_ARG_ENABLE([altivec],
  AS_HELP_STRING([--disable-altivec],
    [disable AltiVec optimizations (default auto)]),, [
  AS_CASE("${host_cpu}", [powerpc*],
        [enable_altivec=yes], [enable_altivec=no])
])
have_altivec="no"
ALTIVEC_CFLAGS=""
AS_IF([test "${enable_altivec}" = "yes"], [
  ARCH="${ARCH} altivec";
  VLC_SAVE_FLAGS
  AC_CACHE_CHECK([if $CC groks AltiVec inline assembly],
    [ac_cv_altivec_inline],
    [AC_TRY_COMPILE(,[asm volatile("vperm 0,1,2,3");],
         ac_cv_altivec_inline="yes",
         [CFLAGS="${CFLAGS} -Wa,-maltivec"
          AC_TRY_COMPILE(,[asm volatile("vperm 0,1,2,3");],
            [ac_cv_altivec_inline="-Wa,-maltivec"],
            ac_cv_altivec_inline=no)
         ])])
  VLC_RESTORE_FLAGS
  AS_IF([test "${ac_cv_altivec_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_ALTIVEC, 1,
              [Define to 1 if AltiVec inline assembly is available.])
    AS_IF([test "${ac_cv_altivec_inline}" != "yes"], [
      ALTIVEC_CFLAGS="${ac_cv_altivec_inline}"
      VLC_ADD_CFLAGS([libvlccore],[${ac_cv_altivec_inline}])
    ])
    have_altivec="yes"
  ])

  VLC_SAVE_FLAGS
  AC_CACHE_CHECK([if \$CC groks AltiVec C extensions],
  [ac_cv_c_altivec], [
    CFLAGS="${CFLAGS} -maltivec"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#include <altivec.h>]], [
[vec_ld(0, (unsigned char *)0);]])], [
      ac_cv_c_altivec="-maltivec"
    ], [
      ac_cv_c_altivec="no"
    ])
  ])
  VLC_RESTORE_FLAGS
  AS_IF([test "${ac_cv_c_altivec}" != "no"], [
    CPPFLAGS="${CPPFLAGS} ${ac_cv_c_altivec}"
    AC_DEFINE(CAN_COMPILE_C_ALTIVEC, 1, [Define to 1 if C AltiVec extensions are available.])
    VLC_ADD_CFLAGS([libvlccore],[${ac_cv_c_altivec}])
    ALTIVEC_CFLAGS="$ALTIVEC_CFLAGS ${ac_cv_c_altivec}"
    VLC_ADD_CFLAGS([deinterlace],[${ac_cv_c_altivec}])
    have_altivec="yes"
  ])
  AC_CHECK_HEADERS([altivec.h])

  VLC_SAVE_FLAGS
  LDFLAGS="${LDFLAGS} -Wl,-framework,vecLib"
  AC_CACHE_CHECK([if linker needs -framework vecLib],
    [ac_cv_ld_altivec],
    [AC_TRY_LINK([],,ac_cv_ld_altivec=yes,ac_cv_ld_altivec=no)])
  VLC_RESTORE_FLAGS
  AS_IF([test "${ac_cv_ld_altivec}" != "no"], [
    VLC_ADD_LIBS([libvlccore],[-Wl,-framework,vecLib])
  ])
])
AC_SUBST([ALTIVEC_CFLAGS])
AM_CONDITIONAL([HAVE_ALTIVEC], [test "$have_altivec" = "yes"])

dnl
dnl  Memory usage
dnl
AC_ARG_ENABLE([optimize-memory],
  AS_HELP_STRING([--enable-optimize-memory],
    [optimize memory usage over performance]))
if test "${enable_optimize_memory}" = "yes"; then
  AC_DEFINE(OPTIMIZE_MEMORY, 1, Define if you want to optimize memory usage over performance)
fi

dnl
dnl Allow running as root (useful for people running on embedded platforms)
dnl
AC_ARG_ENABLE([run-as-root],
  AS_HELP_STRING([--enable-run-as-root],
    [allow running VLC as root (default disabled)]))
AS_IF([test "${enable_run_as_root}" = "yes"],[
    AC_DEFINE(ALLOW_RUN_AS_ROOT, 1,
              [Define to 1 to allow running VLC as root (uid 0).])
])

dnl
dnl Stream output
dnl
AC_ARG_ENABLE([sout],
  AS_HELP_STRING([--disable-sout],
    [disable streaming output (default enabled)]))
AS_IF([test "${enable_sout}" != "no"], [
  AC_DEFINE(ENABLE_SOUT, 1, [Define to 1 for stream output support.])
])
AM_CONDITIONAL([ENABLE_SOUT], [test "${enable_sout}" != "no"])

dnl Lua modules
AC_ARG_ENABLE([lua],
  AS_HELP_STRING([--disable-lua],
    [disable LUA scripting support (default enabled)]))
if test "${enable_lua}" != "no"
then
  PKG_CHECK_MODULES(LUA, lua5.2,
    [ have_lua=yes ],
    [
    AC_MSG_WARN([${LUA_PKG_ERRORS}, trying lua 5.1 instead])

    PKG_CHECK_MODULES(LUA, lua5.1,
      [ have_lua=yes ],
      [
      AC_MSG_WARN([${LUA_PKG_ERRORS}, trying lua >= 5.1 instead])
      PKG_CHECK_MODULES(LUA, lua >= 5.1,
        [ have_lua=yes ],
        [
          AC_MSG_WARN([${LUA_PKG_ERRORS}, trying manual detection instead])
          have_lua=yes
          AC_CHECK_HEADERS([lua.h lauxlib.h lualib.h],
            [],
            [ have_lua=no ] )
          AC_CHECK_LIB(  lua5.2 , luaL_newstate,
            [LUA_LIBS="-llua5.2"],
          AC_CHECK_LIB( lua5.1 , luaL_newstate,
            [LUA_LIBS="-llua5.1"],
            AC_CHECK_LIB( lua51 , luaL_newstate,
              [LUA_LIBS="-llua51"],
              AC_CHECK_LIB( lua , luaL_newstate,
                [LUA_LIBS="-llua"],
                [ have_lua=no
                ], [-lm])
            )))
        ])
      ])
    ])
  if test "x${have_lua}" != "xyes" ;  then
      AC_MSG_ERROR([Could not find lua. Lua is needed for some interfaces (rc, telnet, http) as well as many other custom scripts. Use --disable-lua to ignore this error.])
  fi
  AC_ARG_VAR([LUAC], [LUA byte compiler])
  AS_IF([test -z "$LUAC"], [
     AC_CHECK_TOOL(LUAC, [luac], [false])
  ])
  AS_IF([test "${LUAC}" = "false"], [
    AC_MSG_ERROR([Could not find the LUA byte compiler.])
  ])
  AS_IF([test -d "${CONTRIB_DIR}" -a -f "${CONTRIB_DIR}/lib/liblua.a" -a `echo|${LUAC} -o - -|od -j 8 -N 2 -t x2|head -n 1|tr -s ' '|cut -d' ' -f2` != 0404], [
    AC_MSG_ERROR([You need 32-bits luac when using lua from contrib.])
  ])
fi
AM_CONDITIONAL([BUILD_LUA], [test "${have_lua}" = "yes"])


dnl
dnl VideoLAN manager
dnl
AC_ARG_ENABLE([vlm],
  AS_HELP_STRING([--disable-vlm],
    [disable the stream manager (default enabled)]),,
  [enable_vlm="${enable_sout}"])
AS_IF([test "${enable_vlm}" != "no"], [
  AS_IF([test "${enable_sout}" = "no"], [
    AC_MSG_ERROR([VLM requires the stream output. Do not use --disable-sout.])
  ])
  AC_DEFINE(ENABLE_VLM, 1, [Define if you want the VideoLAN manager support])
])
AM_CONDITIONAL([ENABLE_VLM], [test "${enable_vlm}" != "no"])


dnl
dnl Addons manager storage and repositories modules
dnl
AC_ARG_ENABLE([addonmanagermodules],
  AS_HELP_STRING([--disable-addonmanagermodules],
    [disable the addons manager modules (default enabled)]))
if test "${enable_addonmanagermodules}" != "no"
then
  AC_DEFINE(ENABLE_ADDONMANAGERMODULES, 1, [Define if you want the addons manager modules])
fi
AM_CONDITIONAL([ENABLE_ADDONMANAGERMODULES], [test "${enable_addonmanagermodules}" != "no"])


dnl
dnl  Input plugins
dnl

EXTEND_HELP_STRING([Input plugins:])

dnl
dnl  libarchive access module
dnl
PKG_ENABLE_MODULES_VLC([ARCHIVE], [archive], [libarchive >= 3.1.0], (libarchive support), [auto])

dnl
dnl  live555 input
dnl
AC_ARG_ENABLE([live555],
  AS_HELP_STRING([--enable-live555],
    [enable RTSP input through live555 (default enabled)]))

AS_IF([test "${enable_live555}" != "no" -a -n "${CXX}"], [
  PKG_CHECK_MODULES(LIVE555, live555, [
    VLC_ADD_PLUGIN([live555])
    VLC_ADD_CXXFLAGS([live555], [$LIVE555_CFLAGS])
    VLC_ADD_LIBS([live555],[$LIVE555_LIBS])
  ], [
    AC_MSG_WARN([${LIVE555_PKG_ERRORS}.])

    AC_LANG_PUSH(C++)
    VLC_SAVE_FLAGS

    dnl detect include paths
    AS_IF([test -f "${CONTRIB_DIR}/include/UsageEnvironment/UsageEnvironment.hh"], [
      CPPFLAGS_live555="-I${CONTRIB_DIR}/include/liveMedia -I${CONTRIB_DIR}/include/groupsock -I${CONTRIB_DIR}/include/BasicUsageEnvironment -I${CONTRIB_DIR}/include/UsageEnvironment"
    ], [
      AS_IF([test "${SYS}" != "os2"], [
        LIVE555_PREFIX=${LIVE555_PREFIX-"/usr"}
        CPPFLAGS_live555="-I${LIVE555_PREFIX}/include/liveMedia -I${LIVE555_PREFIX}/include/groupsock -I${LIVE555_PREFIX}/include/BasicUsageEnvironment -I${LIVE555_PREFIX}/include/UsageEnvironment"
      ], [
        LIVE555_PREFIX=${LIVE555_PREFIX-"/usr/lib/live"}
        CPPFLAGS_live555="-I${LIVE555_PREFIX}/liveMedia/include -I${LIVE555_PREFIX}/groupsock/include -I${LIVE555_PREFIX}/BasicUsageEnvironment/include -I${LIVE555_PREFIX}/UsageEnvironment/include"
        LDFLAGS_live555="-L${LIVE555_PREFIX}/liveMedia -L${LIVE555_PREFIX}/groupsock -L${LIVE555_PREFIX}/BasicUsageEnvironment -L${LIVE555_PREFIX}/UsageEnvironment"
      ])
    ])

    dnl CPP Flags
    AS_IF([test "${SYS}" = "solaris"], [
      CPPFLAGS_live555="${CPPFLAGS_live555} -DSOLARIS"
    ])
    CPPFLAGS="${CPPFLAGS} ${CPPFLAGS_live555}"
    LDFLAGS="${LDFLAGS} ${LDFLAGS_live555}"

    dnl version check
    AC_CACHE_CHECK([for live555 version 1324598400 or later], [ac_cv_live555], [
      AC_PREPROC_IFELSE([AC_LANG_PROGRAM([
[#include <liveMedia_version.hh>
#if !defined (LIVEMEDIA_LIBRARY_VERSION_INT) || (LIVEMEDIA_LIBRARY_VERSION_INT < 1324598400)
# error BOOM
#endif]])
      ], [
        ac_cv_live555="yes"
      ], [
        ac_cv_live555="no"
      ])
    ])

    AS_IF([test "$ac_cv_live555" = "no"], [
      AC_MSG_WARN([live555 is missing or its installed version is too old:
Version 2011.12.23 or later is required to proceed.
You can get an updated one from http://www.live555.com/liveMedia .])
      AS_IF([test -n "${enable_live555}"], [
        AC_MSG_ERROR([Update live555 or pass --disable-live555 to disable RTSP input support.])
      ])
    ], [
      other_libs="-lgroupsock -lBasicUsageEnvironment -lUsageEnvironment"
      other_libs_pic="-lgroupsock_pic -lBasicUsageEnvironment_pic -lUsageEnvironment_pic"
      AS_IF([test "${SYS}" = "mingw32"], [
        # add ws2_32 for closesocket, select, recv
        other_libs="$other_libs -lws2_32"
      ])

      dnl We need to check for pic because live555 don't provide shared libs
      dnl and we want to build a plugins so we need -fPIC on some arch.
      VLC_ADD_CXXFLAGS([live555], [${CPPFLAGS_live555}])
      VLC_ADD_LDFLAGS([live555], [${LDFLAGS_live555}])
      AC_CHECK_LIB([liveMedia_pic], [main], [
        VLC_ADD_PLUGIN([live555])
        VLC_ADD_LIBS([live555], [-lliveMedia_pic ${other_libs_pic}])
      ],[
        AC_CHECK_LIB([liveMedia], [main], [
          VLC_ADD_PLUGIN([live555])
          VLC_ADD_LIBS([live555], [-lliveMedia ${other_libs}])
        ],[],[${other_libs}])
      ],[${other_libs_pic}])
    ])
    VLC_RESTORE_FLAGS
    AC_LANG_POP(C++)
  ])
])

dnl
dnl IIDC and DV FireWire input modules
dnl
PKG_ENABLE_MODULES_VLC([DC1394], [], [libdc1394-2 >= 2.1.0], [IIDC FireWire input module], [auto])
PKG_ENABLE_MODULES_VLC([DV1394], [], [libraw1394 >= 2.0.1 libavc1394 >= 0.5.3], [DV FireWire input module], [auto])

dnl
dnl - linsys modules: access module check for libzvbi
dnl
AC_ARG_ENABLE([linsys],
  AS_HELP_STRING([--enable-linsys],
    [Linux Linear Systems Ltd. SDI and HD-SDI input cards (default enabled)]))
AS_IF([test "$SYS" = "linux" -a "${enable_linsys}" != "no"], [
  VLC_ADD_PLUGIN([linsys_hdsdi])
  PKG_CHECK_MODULES(LINSYS_SDI, [zvbi-0.2 >= 0.2.28], [
    VLC_ADD_PLUGIN([linsys_sdi])
  ], [
    AC_MSG_WARN([${LINSYS_SDI_PKG_ERRORS}.])
  ])
])

dnl
dnl dvdread module: check for libdvdread
dnl
PKG_ENABLE_MODULES_VLC([DVDREAD], [], [dvdread > 6.0.0], [dvdread input module], [auto])

dnl
dnl libdvdnav plugin
dnl
PKG_ENABLE_MODULES_VLC([DVDNAV], [], [dvdnav >= 6.0.0], [DVD with navigation input module (dvdnav)], [auto])

dnl
dnl  Blu-ray Disc Support with libbluray
dnl
PKG_ENABLE_MODULES_VLC([BLURAY], [libbluray], [libbluray >= 0.6.2], (libbluray for Blu-ray disc support ) )

dnl
dnl  OpenCV wrapper and example filters
dnl
PKG_ENABLE_MODULES_VLC([OPENCV], [opencv_example opencv_wrapper], [opencv > 2.0], (OpenCV (computer vision) filter), [auto])


dnl
dnl  libsmbclient plugin
dnl
PKG_ENABLE_MODULES_VLC([SMBCLIENT], [smbc], [smbclient], (SMB/CIFS support), [auto])

dnl
dnl  liBDSM access module
dnl
AM_CONDITIONAL([HAVE_DSM], [test "$AS_TR_SH(with_dsm)" = "yes"])
PKG_WITH_MODULES([DSM], [libdsm >= 0.2.0], [
       VLC_ADD_PLUGIN([dsm])
       VLC_ADD_CFLAGS([dsm], [$DSM_CFLAGS])
       VLC_ADD_LIBS([dsm], [$DSM_LIBS])
       have_dsm="yes"
   ],,[libdsm SMB/CIFS access/sd module], [auto])
AM_CONDITIONAL([HAVE_DSM], [test "${have_dsm}" = "yes"])

dnl
dnl sftp access support
dnl
PKG_ENABLE_MODULES_VLC([SFTP], [sftp], [libssh2], (support SFTP file transfer via libssh2), [auto])

dnl
dnl nfs access support
dnl
PKG_ENABLE_MODULES_VLC([NFS], [nfs], [libnfs >= 1.10.0], (support nfs protocol via libnfs), [auto])

dnl
dnl smb2 access support
dnl
PKG_ENABLE_MODULES_VLC([SMB2], [smb2], [libsmb2 >= 3.0.0], (support smb2 protocol via libsmb2), [auto])

dnl
dnl  Video4Linux 2
dnl
AC_ARG_ENABLE([v4l2], AS_HELP_STRING([--disable-v4l2],
  [disable Video4Linux version 2 (default auto)]))
have_v4l2="no"
AS_IF([test "$enable_v4l2" != "no"], [
  AC_CHECK_HEADERS([linux/videodev2.h], [
    have_v4l2="yes"
  ])
])
AM_CONDITIONAL([HAVE_V4L2], [test "${have_v4l2}" != "no"])

dnl
dnl  NVDEC decoder/encoder
dnl
AC_ARG_ENABLE([nvdec], AS_HELP_STRING([--disable-nvdec],
  [disable NVDEC decoder (default auto)]))
have_nvdec="no"
AS_IF([test "$enable_nvdec" != "no"], [
  AC_CHECK_HEADERS([ffnvcodec/dynlink_loader.h], [
    have_nvdec="yes"
  ])
])
AM_CONDITIONAL([HAVE_NVDEC], [test "${have_nvdec}" != "no"])

dnl
dnl special access module for Blackmagic SDI cards
dnl
AC_ARG_ENABLE([decklink],
  AS_HELP_STRING([--disable-decklink],
    [disable Blackmagic DeckLink SDI input (default auto)]))
AC_ARG_WITH([decklink_sdk],
  AS_HELP_STRING([--with-decklink-sdk=DIR],
    [location of Blackmagic DeckLink SDI SDK]))
have_decklink=no
if test "${enable_decklink}" != "no"
then
  if test "${with_decklink_sdk}" != "no" -a -n "${with_decklink_sdk}"
  then
    VLC_ADD_CPPFLAGS([decklink decklinkoutput stream_out_sdi],[-I${with_decklink_sdk}/include])
  fi
  VLC_SAVE_FLAGS
  CPPFLAGS="${CPPFLAGS} ${CPPFLAGS_decklink}"
  AC_LANG_PUSH([C++])
  AC_CHECK_HEADERS([DeckLinkAPIDispatch.cpp], [
      have_decklink=yes
      AS_IF([test "${SYS}" = "darwin"], [
        VLC_ADD_LIBS([decklink decklinkoutput],[-Wl,-framework,CoreFoundation])
      ])
  ], [
      AS_IF([test "${enable_decklink}" = "yes"], [
        AC_MSG_ERROR([Blackmagic DeckLink SDI include files not found!])
      ], [
        AC_MSG_WARN([Blackmagic DeckLink SDI include files not found, decklink disabled])
      ])
  ])
  AC_LANG_POP([C++])
  VLC_RESTORE_FLAGS
fi
AM_CONDITIONAL([HAVE_DECKLINK], [ test "${have_decklink}" != "no" ])


dnl
dnl  Built-in CD-DA and VCD module
dnl
AC_ARG_ENABLE([vcd],
  AS_HELP_STRING([--disable-vcd],
    [disable built-in VCD and CD-DA support (default enabled)]))

AC_ARG_ENABLE([libcddb],
  AS_HELP_STRING([--disable-libcddb],
    [disable CDDB for Audio CD (default enabled)]))

if test "${enable_vcd}" != "no" -a "${enable_winstore_app}" != "yes"
then
  AC_MSG_CHECKING(for cdrom_msf0 in linux/cdrom.h)
  AC_EGREP_HEADER(cdrom_msf0,linux/cdrom.h,[
    AC_MSG_RESULT(yes)
    VLC_ADD_PLUGIN([vcd cdda])
  ],[
    AC_MSG_RESULT(no)
  ])

  AC_MSG_CHECKING(for scsireq in sys/scsiio.h)
  AC_EGREP_HEADER(scsireq,sys/scsiio.h,[
    AC_MSG_RESULT(yes)
    VLC_ADD_PLUGIN([vcd cdda])
    AC_DEFINE(HAVE_SCSIREQ_IN_SYS_SCSIIO_H, 1, For NetBSD VCD support)
  ],[
    AC_MSG_RESULT(no)
  ])

  AC_MSG_CHECKING(for ioc_toc_header in sys/cdio.h)
  AC_EGREP_HEADER(ioc_toc_header ,sys/cdio.h,[
    AC_MSG_RESULT(yes)
    VLC_ADD_PLUGIN([vcd cdda])
    AC_DEFINE(HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H, 1, For FreeBSD VCD support)
  ],[
    AC_MSG_RESULT(no)
  ])

  if test "${SYS}" = "bsdi" -o "${SYS}" = "mingw32" -o "${SYS}" = "os2" -o "${SYS}" = "darwin"
  then
    VLC_ADD_PLUGIN([vcd cdda])
  fi

  if test "$enable_libcddb" != "no"; then
    PKG_CHECK_MODULES(LIBCDDB, [libcddb >= 0.9.5], [
      HAVE_LIBCDDB=yes
      AC_DEFINE(HAVE_LIBCDDB, 1, [Define this if you have libcddb installed])
      VLC_ADD_LIBS([cdda],[$LIBCDDB_LIBS])
      VLC_ADD_CFLAGS([cdda],[$LIBCDDB_CFLAGS])
      ],:
      [AC_MSG_WARN([${LIBCDDB_PKG_ERRORS}: CDDB access disabled.])
      HAVE_LIBCDDB=no])
  fi
fi

dnl
dnl Linux DVB
dnl
AC_CACHE_CHECK([for Linux DVB version 5.1], [ac_cv_linux_dvb_5_1], [
  AC_PREPROC_IFELSE([AC_LANG_PROGRAM([
[#include <linux/dvb/version.h>
#if (DVB_API_VERSION < 5)
# error Linux DVB API v3.2 and older are not supported.
#endif
#if (DVB_API_VERSION == 5 && DVB_API_VERSION_MINOR < 1)
# error Linux DVB API v5.0 is unsupported. Please update.
#endif
]])], [
  ac_cv_linux_dvb_5_1=yes
], [
  ac_cv_linux_dvb_5_1=no
])])
AM_CONDITIONAL([HAVE_LINUX_DVB], [test "$ac_cv_linux_dvb_5_1" = "yes"])

dnl
dnl  Screen capture module
dnl
AC_ARG_ENABLE([screen],
  AS_HELP_STRING([--enable-screen],
    [disable screen capture (default enabled)]))
if test "${enable_screen}" != "no"; then
  if test "${SYS}" = "darwin"; then
    AC_CHECK_HEADERS([ApplicationServices/ApplicationServices.h], [
      VLC_ADD_PLUGIN([screen])
    ])
  fi
fi
AM_CONDITIONAL([HAVE_MAC_SCREEN], [test "${SYS}" = "darwin" -a "x${enable_screen}" != "xno"])

dnl
dnl  VNC/RFB access module
dnl
PKG_ENABLE_MODULES_VLC([VNC], [vnc], [libvncclient >= 0.9.9], (VNC/rfb client support), [auto])

dnl  RDP/Remote Desktop access module
dnl
PKG_ENABLE_MODULES_VLC([FREERDP], [rdp], [freerdp >= 1.0.1], (RDP/Remote Desktop client support) )

dnl
dnl AVFoundation
AC_SUBST([have_avfoundation], ["no"])
AC_ARG_ENABLE([macosx-avfoundation],
  AS_HELP_STRING([--enable-macosx-avfoundation], [Mac OS X avcapture (video) module (default enabled on Mac OS X)]))
if test "x${enable_macosx_avfoundation}" != "xno" &&
  (test "${SYS}" = "darwin" || test "${enable_macosx_avfoundation}" = "yes")
then
  SAVED_LIBS="${LIBS}"
  LIBS="-framework AVFoundation"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [])],[have_avfoundation=yes],[have_avfoundation=no])
  LIBS="${SAVED_LIBS}"
  if test "${have_avfoundation}" != "no"
  then
    VLC_ADD_PLUGIN([avcapture])
  fi
fi
AM_CONDITIONAL([HAVE_AVFOUNDATION], [test "${have_avfoundation}" != "no"])

dnl
dnl  DCP plugin (using asdcplib)
dnl
PKG_WITH_MODULES([ASDCP], [asdcplib], [have_asdcp="yes"])
AM_CONDITIONAL([HAVE_ASDCP], [test "${have_asdcp}" = "yes"])

dnl
dnl  Demux plugins
dnl

EXTEND_HELP_STRING([Mux/Demux plugins:])

dnl
dnl  libdvbpsi check for ts mux/demux
dnl
have_dvbpsi="no"
PKG_WITH_MODULES([DVBPSI], [libdvbpsi >= 1.2.0], [have_dvbpsi="yes"])
AM_CONDITIONAL([HAVE_DVBPSI], [test "${have_dvbpsi}" = "yes"])


dnl
dnl  GME demux plugin
dnl
AC_ARG_ENABLE([gme],
  AS_HELP_STRING([--enable-gme],
    [use Game Music Emu (default auto)]))
AS_IF([test "${enable_gme}" != "no"], [
  AC_CHECK_HEADER([gme/gme.h], [
    AC_CHECK_LIB([gme], [gme_identify_header], [
      VLC_ADD_PLUGIN([gme])
    ], [
      AC_CHECK_LIB([gme], [gme_identify_extension], [
         VLC_ADD_LIBS([gme], [-lstdc++ $LIBM])
         VLC_ADD_PLUGIN([gme])
      ],, [-lstdc++ $LIBM])
    ])
    VLC_ADD_LIBS([gme], [-lgme])
  ], [
    AS_IF([test "x${enable_gme}" != "x"], [
      AC_MSG_ERROR([GME cannot be found. Please install the development files.])
    ])
  ])
])


dnl
dnl  SIDPlay plugin
dnl
AC_ARG_ENABLE([sid],
    AS_HELP_STRING([--enable-sid],[C64 sid demux support (default auto)]))
AS_IF([test "${enable_sid}" != "no"], [
    PKG_CHECK_MODULES(SID, [libsidplay2], [
        AC_LANG_PUSH(C++)
        oldCPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $SID_CFLAGS"
        AC_CHECK_HEADER([sidplay/builders/resid.h], [
            VLC_ADD_PLUGIN([sid])
            VLC_ADD_CFLAGS([sid], [$SID_CFLAGS])
            VLC_ADD_LIBS([sid], [$SID_LIBS] [-lresid-builder])
        ], [
            AS_IF([test -n "${enable_sid}"],
                [AC_MSG_ERROR([${SID_PKG_ERRORS} (required for sid).])],
                [AC_MSG_WARN([${SID_PKG_ERRORS} (required for sid).])]
                )
        ])
        CPPFLAGS="$oldCPPFLAGS"
        AC_LANG_POP(C++)
    ], [
        AS_IF([test "x${enable_sid}" = "xyes"],
            [AC_MSG_ERROR([${SID_PKG_ERRORS} (required for sid).])],
            [AC_MSG_WARN([${SID_PKG_ERRORS} (required for sid).])]
            )
    ])
])


dnl
dnl  ogg demux plugin
dnl
PKG_ENABLE_MODULES_VLC([OGG], [mux_ogg], [ogg >= 1.0], [Ogg demux support], [auto])
PKG_CHECK_MODULES(LIBVORBIS, [vorbis >= 1.1], [
    AC_DEFINE(HAVE_LIBVORBIS, 1, [Define to 1 if you have the libvorbis])
],[true])
PKG_ENABLE_MODULES_VLC([OGG], [], [ogg >= 1.0], [Ogg demux support], [auto], [${LIBVORBIS_CFLAGS}], [${LIBVORBIS_LIBS}])
if test "${enable_sout}" != "no"; then
dnl Check for libshout
    PKG_ENABLE_MODULES_VLC([SHOUT], [access_output_shout], [shout >= 2.1], [libshout output plugin], [auto])
fi

dnl
dnl  matroska demux plugin
dnl
PKG_WITH_MODULES([MATROSKA], [libebml >= 1.3.6 libmatroska], [
       VLC_ADD_PLUGIN([mkv])
       VLC_ADD_CFLAGS([mkv], [$MATROSKA_CFLAGS])
       VLC_ADD_LIBS([mkv], [$MATROSKA_LIBS])
       have_matroska="yes"
   ],,[MKV format support], [auto])
AM_CONDITIONAL([HAVE_MATROSKA], [test "${have_matroska}" = "yes"])

dnl
dnl  modplug demux plugin
dnl
AC_ARG_ENABLE([mod],
  AS_HELP_STRING([--disable-mod],
    [do not use libmodplug (default auto)]))
if test "${enable_mod}" != "no" ; then
    PKG_CHECK_MODULES(LIBMODPLUG, [libmodplug >= 0.8.9.0], [
          VLC_ADD_PLUGIN([mod])
          VLC_ADD_CXXFLAGS([mod],[$LIBMODPLUG_CFLAGS])
          VLC_ADD_CFLAGS([mod],[$LIBMODPLUG_CFLAGS]) #modules/demux/mod.c needs CFLAGS_mod, not CXXFLAGS_mod
          VLC_ADD_LIBS([mod],[$LIBMODPLUG_LIBS])
    ],[
       AS_IF([test -n "${enable_mod}"],
         [AC_MSG_ERROR([${LIBMODPLUG_PKG_ERRORS}.])],
         [AC_MSG_WARN([${LIBMODPLUG_PKG_ERRORS}.])])
    ])
fi

dnl
dnl  mpc demux plugin
dnl
AC_ARG_ENABLE([mpc],
  AS_HELP_STRING([--disable-mpc], [do not use libmpcdec (default auto)]))
if test "${enable_mpc}" != "no"
then
    AC_CHECK_HEADERS([mpcdec/mpcdec.h], [
    VLC_ADD_PLUGIN([mpc])
    VLC_ADD_LIBS([mpc],[-lmpcdec])])
fi

dnl
dnl  Codec plugins
dnl

EXTEND_HELP_STRING([Codec plugins:])

dnl
dnl shine encoder plugin
dnl
PKG_ENABLE_MODULES_VLC([SHINE], [], [shine >= 3.0.0], [MPEG Audio Layer 3 encoder], [auto], [], [], [])


dnl
dnl openmax il codec plugin
dnl
AC_ARG_ENABLE([omxil],
  AS_HELP_STRING([--enable-omxil], [openmax il codec module (default disabled)]))
if test "${enable_omxil}" = "yes"
then
  VLC_ADD_PLUGIN([omxil])
fi

dnl
dnl raspberry pi openmax il configuration
dnl
AC_ARG_ENABLE([rpi-omxil],
  AS_HELP_STRING([--enable-rpi-omxil], [openmax il configured for raspberry pi (default disabled)]))
if test "${enable_rpi_omxil}" = "yes"
then
  VLC_ADD_PLUGIN([omxil])
  VLC_ADD_CFLAGS([omxil],[-DRPI_OMX])
fi

dnl
dnl  mad plugin
dnl
have_mad="no"
MAD_CFLAGS=""
MAD_LIBS=""

AC_ARG_WITH(mad, AS_HELP_STRING([--with-mad=PATH], [path to libmad]), [
  enable_mad="yes"
], [
  with_mad="no"
])
AS_IF([test "${with_mad}" != "no"], [
  MAD_CFLAGS="-I${with_mad}/include"
  MAD_LIBS="-L${with_mad}/lib"
])

AC_ARG_ENABLE([mad],
  AS_HELP_STRING([--enable-mad], [libmad module (default enabled)]))
AS_IF([test "${enable_mad}" != "no"], [
  VLC_SAVE_FLAGS
  CPPFLAGS="${CPPFLAGS} ${MAD_CFLAGS}"
  LDFLAGS="${LDFLAGS} ${MAD_LIBS}"
  AC_CHECK_HEADERS([mad.h], [
    AC_CHECK_LIB([mad], [mad_bit_init], [
      have_mad="yes"
      MAD_LIBS="${MAD_LIBS} -lmad"
    ])
  ])
  VLC_RESTORE_FLAGS
  AS_IF([test -n "$enable_mad" -a "${have_mad}" = "no"], [
    AC_MSG_ERROR([Could not find libmad. Install it or pass --disable-mad to disable it.])
  ])
])
AC_SUBST([MAD_CFLAGS])
AC_SUBST([MAD_LIBS])
AM_CONDITIONAL([HAVE_MAD], [test "${have_mad}" = "yes"])

dnl  mpg123 plugin
dnl
dnl
PKG_ENABLE_MODULES_VLC([MPG123], [mpg123], [libmpg123], [libmpg123 decoder support], [auto])

dnl
dnl gstreamer stuff
dnl
AC_ARG_ENABLE([gst-decode],
  AS_HELP_STRING([--enable-gst-decode], [GStreamer based decoding support (currently supports only video decoding) (default auto)]))

have_gst_decode="no"
AS_IF([test "${enable_gst_decode}" != "no"], [
  PKG_CHECK_MODULES([GST_APP], [gstreamer-app-1.0], [
    PKG_CHECK_MODULES([GST_VIDEO], [gstreamer-video-1.0], [
      have_gst_decode="yes"
    ], [
      AC_MSG_WARN([${GST_VIDEO_PKG_ERRORS}. GStreamer decoder module will not be built.])
    ])
  ], [
    AC_MSG_WARN([${GST_APP_PKG_ERRORS}. GStreamer modules will not be built.])
  ])
])
AM_CONDITIONAL([HAVE_GST_DECODE], [test "${have_gst_decode}" = "yes"])

dnl
dnl  avcodec decoder/encoder plugin
dnl
AC_ARG_ENABLE([merge-ffmpeg],
AS_HELP_STRING([--enable-merge-ffmpeg], [merge FFmpeg-based plugins (default disabled)]),, [
  enable_merge_ffmpeg="no"
])
AM_CONDITIONAL([MERGE_FFMPEG], [test "$enable_merge_ffmpeg" != "no"])

AC_MSG_CHECKING([for libavutil variant])
PKG_CHECK_EXISTS([libavutil], [
  libavutil_version="$(${PKG_CONFIG} --modversion libavutil)"
  libavutil_micro="${libavutil_version##*.}"
  AS_IF([test ${libavutil_micro} -le 99], [
    avfork="libav"
  ], [
    avfork="ffmpeg"
  ])
], [
  avfork="none"
])
AC_MSG_RESULT([${avfork}])

AC_ARG_ENABLE([avcodec],
AS_HELP_STRING([--enable-avcodec], [libavcodec codec (default enabled)]))
AS_IF([test "${enable_avcodec}" != "no"], [
  dnl We need >=57.16.0 from libav or >=57.37.100 from ffmpeg
  case "${avfork}" in
    libav) av_codec_ver="57.16.0" ;;
    dnl 05f66706d182eb0c36af54d72614bf4c33e957a9  includes libavutil 55.9.0
    ffmpeg) av_codec_ver="57.37.100" ;;
    dnl 7fc329e2dd6226dfecaa4a1d7adf353bf2773726  include libavutil 55.22.101
    *) AC_MSG_ERROR([Missing libav or FFmpeg. Pass --disable-avcodec to ignore this error.])
  esac
  PKG_CHECK_MODULES(AVCODEC,[libavcodec >= ${av_codec_ver} libavutil >= 55.9.0], [
    VLC_SAVE_FLAGS
    CPPFLAGS="${CPPFLAGS} ${AVCODEC_CFLAGS}"
    CFLAGS="${CFLAGS} ${AVCODEC_CFLAGS}"
    AC_CHECK_HEADERS([libavcodec/avcodec.h])
    AC_CHECK_HEADERS([libavutil/avutil.h])
    VLC_RESTORE_FLAGS
    have_avcodec="yes"
  ],[
    AC_MSG_ERROR([${AVCODEC_PKG_ERRORS}. Pass --disable-avcodec to ignore this error.])
  ])
], [
  have_avcodec="no"
])
AM_CONDITIONAL([HAVE_AVCODEC], [test "${have_avcodec}" != "no"])

dnl
dnl libva needs avcodec
dnl
AC_ARG_ENABLE([libva],
  AS_HELP_STRING([--enable-libva], [VAAPI GPU decoding support (libVA) (default auto)]))

AS_IF([test "${enable_libva}" = "yes" -a "${have_avcodec}" != "yes" ], [
  AC_MSG_ERROR([--enable-libva and --disable-avcodec options are mutually exclusive. Use --enable-avcodec.])
])

have_vaapi="no"
have_vaapi_drm="no"
have_vaapi_x11="no"
have_vaapi_wl="no"
AS_IF([test "${enable_libva}" != "no"], [
  PKG_CHECK_MODULES([LIBVA], [libva >= 1.0], [
    have_vaapi="yes"
  ], [
    AS_IF([test -n "${enable_libva}"], [
      AC_MSG_ERROR([${LIBVA_PKG_ERRORS}.])
    ], [
      AC_MSG_WARN([${LIBVA_PKG_ERRORS}.])
    ])
  ])
  PKG_CHECK_MODULES([LIBVA_DRM], [libva-drm], [
    have_vaapi_drm="yes"
  ], [
    AC_MSG_WARN([${LIBVA_DRM_PKG_ERRORS}.])
  ])
  PKG_CHECK_MODULES(LIBVA_X11, [libva-x11], [
    have_vaapi_x11="yes"
  ], [
    AC_MSG_WARN([${LIBVA_X11_PKG_ERRORS}.])
  ])
  PKG_CHECK_MODULES(LIBVA_WL, [libva-wayland], [
    have_vaapi_wl="yes"
  ], [
    AC_MSG_WARN([${LIBVA_WL_PKG_ERRORS}.])
  ])
])
AM_CONDITIONAL([HAVE_VAAPI], [test "${have_vaapi}" = "yes"])
AM_CONDITIONAL([HAVE_VAAPI_DRM], [test "${have_vaapi_drm}" = "yes"])
AM_CONDITIONAL([HAVE_VAAPI_X11], [test "${have_vaapi_x11}" = "yes"])
AM_CONDITIONAL([HAVE_VAAPI_WL], [test "${have_vaapi_wl}" = "yes"])

have_avcodec_vaapi="no"
AS_IF([test "${have_vaapi}" = "yes" -a "${have_avcodec}" = "yes"], [
  VLC_SAVE_FLAGS
  CPPFLAGS="${CPPFLAGS} ${AVCODEC_CFLAGS}"
  CFLAGS="${CFLAGS} ${AVCODEC_CFLAGS}"
  AC_CHECK_HEADERS([libavcodec/vaapi.h], [
    AC_MSG_NOTICE([VA API acceleration activated])
    have_avcodec_vaapi="yes"
  ],[
    AS_IF([test -n "${enable_libva}"], [
      AC_MSG_ERROR([libva is present but libavcodec/vaapi.h is missing])
    ], [
      AC_MSG_WARN([libva is present but libavcodec/vaapi.h is missing ])
    ])
  ])
  VLC_RESTORE_FLAGS
])
AM_CONDITIONAL([HAVE_AVCODEC_VAAPI], [test "${have_avcodec_vaapi}" = "yes"])

dnl
dnl dxva2 needs avcodec
dnl
AC_ARG_ENABLE([dxva2],
  AS_HELP_STRING([--enable-dxva2], [DxVA2 GPU decoding support (default auto)]))

have_avcodec_dxva2="no"
AS_IF([test "${enable_dxva2}" != "no"], [
  if test "${SYS}" = "mingw32"; then
  AS_IF([test "x${have_avcodec}" = "xyes"], [
    AC_CHECK_HEADERS([dxva2api.h],
      [
        AC_CHECK_HEADERS([libavcodec/dxva2.h], [
           AC_MSG_NOTICE([DxVA2 acceleration activated])
           have_avcodec_dxva2="yes"
        ],[
    AS_IF([test "${enable_dxva2}" = "yes"],
          [AC_MSG_ERROR([dxva2 is present but libavcodec/dxva2.h is missing])],
              [AC_MSG_WARN([dxva2 is present but libavcodec/dxva2.h is missing ])])
        ], [#undef _WIN32_WINNT
            /* DXVA2 is only available in Vista and above */
            #define _WIN32_WINNT 0x600])
      ],[
    AS_IF([test "${enable_dxva2}" = "yes"],
              [AC_MSG_ERROR([Could not find required dxva2api.h])],
              [AC_MSG_WARN([dxva2api.h not found])])
      ],[#include <d3d9.h>])
  ],[
    AS_IF([test "x${enable_dxva2}" != "x"], [
      AC_MSG_ERROR([--enable-dxva2 and --disable-avcodec options are mutually exclusive.])
    ])
  ])
  fi
])
AM_CONDITIONAL([HAVE_AVCODEC_DXVA2], [test "${have_avcodec_dxva2}" = "yes"])

dnl
dnl d3d11va needs avcodec
dnl
AC_ARG_ENABLE([d3d11va],
  AS_HELP_STRING([--enable-d3d11va], [D3D11 GPU decoding support (default auto)]))

have_avcodec_d3d11va="no"
AS_IF([test "${enable_d3d11va}" != "no"], [
  if test "${SYS}" = "mingw32"; then
  AS_IF([test "x${have_avcodec}" = "xyes"], [
        AC_CHECK_TYPES([ID3D11VideoDecoder],
          [
            AC_CHECK_HEADERS([dxva2api.h],
              [
                AC_CHECK_HEADERS([libavcodec/d3d11va.h], [
                   AC_MSG_NOTICE([D3D11 acceleration activated])
                   AC_DEFINE([HAVE_AVCODEC_D3D11VA], [1], [Define if the d3d11va module is built])
                   have_avcodec_d3d11va="yes"
                ],[
                  AS_IF([test "${enable_d3d11va}" = "yes"],
                    [AC_MSG_ERROR([d3d11va is present but libavcodec/d3d11va.h is missing])],
                        [AC_MSG_WARN([d3d11va is present but libavcodec/d3d11va.h is missing ])])
                  ], [#undef _WIN32_WINNT
                      /* D3D11 is only available in Vista and above */
                      #define _WIN32_WINNT 0x600])
                  ],[
                    AS_IF([test "${enable_d3d11va}" = "yes"],
                          [AC_MSG_ERROR([Could not find required dxva2api.h])],
                          [AC_MSG_WARN([dxva2api.h not found])])
                  ],[#include <d3d9.h>])
          ],[
            AS_IF([test "${enable_d3d11va}" = "yes"],
                  [AC_MSG_ERROR([Could not find required ID3D11VideoDecoder in d3d11.h])],
                  [AC_MSG_WARN([ID3D11VideoDecoder not found])])
          ], [#include <d3d11.h>])
  ],[
    AS_IF([test "x${enable_d3d11va}" != "x"], [
      AC_MSG_ERROR([--enable-d3d11va and --disable-avcodec options are mutually exclusive.])
    ])
  ])
  fi
])
AM_CONDITIONAL([HAVE_AVCODEC_D3D11VA], [test "${have_avcodec_d3d11va}" = "yes"])

dnl
dnl DXGI debug
dnl
AC_CHECK_HEADERS([dxgidebug.h dxgi1_6.h])

dnl
dnl IApplicationData2
dnl
have_iapplication_data2="no"
AC_CHECK_TYPES([IApplicationData2],[],[],[#include <windows.storage.h>])

dnl
dnl DirectComposition
dnl
AC_LANG_PUSH([C++])
AC_CHECK_HEADERS([dcomp.h], [
    have_dcomp="yes"
], [], [
  #include <windows.h>
])
AC_LANG_POP([C++])
AM_CONDITIONAL([HAVE_DCOMP], [test "${have_dcomp}" = "yes"])


dnl
dnl  avformat demuxer/muxer plugin
dnl

AC_ARG_ENABLE([avformat],
AS_HELP_STRING([--enable-avformat], [libavformat containers (default enabled)]),, [
  enable_avformat="${have_avcodec}"
])
have_avformat="no"
AS_IF([test "${enable_avformat}" != "no"], [
  PKG_CHECK_MODULES(AVFORMAT,[libavformat >= 53.21.0 libavcodec libavutil], [
      have_avformat="yes"
      VLC_SAVE_FLAGS
      CPPFLAGS="${CPPFLAGS} ${AVFORMAT_CFLAGS}"
      CFLAGS="${CFLAGS} ${AVFORMAT_CFLAGS}"
      AC_CHECK_HEADERS([libavformat/avformat.h libavformat/avio.h])
      AC_CHECK_HEADERS([libavcodec/avcodec.h])
      AC_CHECK_HEADERS([libavutil/avutil.h])
      AS_IF([test "$enable_merge_ffmpeg" = "no"], [
        have_avformat="no"
      ])
      VLC_RESTORE_FLAGS
    ],[
      AC_MSG_ERROR([${AVFORMAT_PKG_ERRORS}. Pass --disable-avformat to ignore this error.])
  ])
])
AM_CONDITIONAL([HAVE_AVFORMAT], [test "${enable_avformat}" != "no"])

dnl
dnl  swscale image scaling and conversion plugin
dnl

AC_ARG_ENABLE([swscale],
  AS_HELP_STRING([--enable-swscale],[libswscale image scaling and conversion
          (default enabled)]))
if test "${enable_swscale}" != "no"
then
  PKG_CHECK_MODULES(SWSCALE,[libswscale],
    [
      VLC_SAVE_FLAGS
      CPPFLAGS="${CPPFLAGS} ${SWSCALE_CFLAGS}"
      CFLAGS="${CFLAGS} ${SWSCALE_CFLAGS}"
      AC_CHECK_HEADERS([libswscale/swscale.h])
      AC_CHECK_HEADERS([libavutil/avutil.h])
      VLC_ADD_PLUGIN([swscale])
      VLC_ADD_LIBS([swscale],[$SWSCALE_LIBS])
      VLC_ADD_CFLAGS([swscale],[$SWSCALE_CFLAGS])
      AS_IF([test -n "${ac_cv_ld_bsymbolic}"], [
        VLC_ADD_LIBS([swscale],[${ac_cv_ld_bsymbolic}])
      ])
      VLC_RESTORE_FLAGS
    ],[
      AC_MSG_ERROR([${SWSCALE_PKG_ERRORS}. Pass --disable-swscale to ignore this error. Proper software scaling and some video chroma conversion will be missing.])
  ])
fi

dnl
dnl  postproc plugin
dnl

AC_ARG_ENABLE([postproc],
AS_HELP_STRING([--enable-postproc], [libpostproc image post-processing (default auto)]))
if test "${enable_postproc}" != "no"
then
  PKG_CHECK_MODULES(POSTPROC,[libpostproc libavutil],
    [
      VLC_SAVE_FLAGS
      CPPFLAGS="${CPPFLAGS} ${POSTPROC_CFLAGS}"
      CFLAGS="${CFLAGS} ${POSTPROC_CFLAGS}"
      AC_CHECK_HEADERS([postproc/postprocess.h])
      VLC_ADD_PLUGIN([postproc])
      VLC_RESTORE_FLAGS
    ],[
      AC_MSG_WARN([${POSTPROC_PKG_ERRORS}.])
  ])
fi

dnl
dnl  faad decoder plugin
dnl
PKG_WITH_MODULES([FAAD],[faad2],
  VLC_ADD_PLUGIN(faad)
  VLC_ADD_CFLAGS(faad, [$FAAD_CFLAGS])
  VLC_ADD_LIBS(faad,[$FAAD_LIBS]),
  AS_IF([test "x${enable_faad}" != "xno"],[
    AC_CHECK_HEADERS([neaacdec.h], [
      AC_CHECK_LIB([faad], [NeAACDecOpen],[
                      VLC_ADD_PLUGIN([faad])
                      VLC_ADD_LIBS([faad],[-lfaad])
                      found_faad="yes"
                   ],[], [$LIBM])
    ])
    AS_IF([test "x${found_faad}" != "xyes"],
        AS_IF([test "x${enable_faad}" == "xyes"],
            AC_MSG_ERROR([${FAAD_PKG_ERRORS}.]),
            AC_MSG_WARN([${FAAD_PKG_ERRORS}.]))
    )
  ]),
[faad AAC audio decoder])

dnl
dnl  AOM encoder/decoder plugin
dnl
PKG_ENABLE_MODULES_VLC([AOM], [], [aom], [libaom AV1 encoder and decoder (default auto)])

dnl
dnl  Dav1d decoder plugin
dnl
PKG_ENABLE_MODULES_VLC([DAV1D], [], [dav1d >= 0.5.0], [AV1 decoder (default auto)])

dnl
dnl libvpx decoder plugin
dnl
AC_ARG_ENABLE([vpx],
    AS_HELP_STRING([--enable-vpx],[libvpx VP8/VP9 encoder and decoder (default auto)]))
AS_IF([test "${enable_vpx}" != "no"],[
    PKG_CHECK_MODULES([VPX], [vpx >= 1.5.0], [
        VLC_ADD_PLUGIN([vpx])
        VLC_ADD_CPPFLAGS([vpx], [${VPX_CFLAGS}])
        VLC_ADD_LIBS([vpx], [${VPX_LIBS}])
        AC_CHECK_LIB([vpx],[vpx_codec_vp8_dx], [
            VLC_ADD_CPPFLAGS([vpx], [-DENABLE_VP8_DECODER])
        ], [], [${VPX_LIBS}])
        AC_CHECK_LIB([vpx],[vpx_codec_vp9_dx], [
            VLC_ADD_CPPFLAGS([vpx], [-DENABLE_VP9_DECODER])
        ], [], [${VPX_LIBS}])
        AC_CHECK_LIB([vpx],[vpx_codec_vp8_cx], [
            VLC_ADD_CPPFLAGS([vpx], [-DENABLE_VP8_ENCODER])
        ], [], [${VPX_LIBS}])
        AC_CHECK_LIB([vpx],[vpx_codec_vp9_cx], [
            VLC_ADD_CPPFLAGS([vpx], [-DENABLE_VP9_ENCODER])
        ], [], [${VPX_LIBS}])
    ], [
    AS_IF([test "${enable_vpx}" = "yes"],[
        AC_MSG_ERROR([libvpx 1.5.0 or higher was not found])
    ])])
])

dnl
dnl twolame encoder plugin
dnl
PKG_ENABLE_MODULES_VLC([TWOLAME], [], [twolame], [MPEG Audio Layer 2 encoder], [auto], [], [], [ -DLIBTWOLAME_STATIC])

dnl
dnl fdk-aac encoder plugin
dnl
PKG_ENABLE_MODULES_VLC([FDKAAC], [], [fdk-aac], [FDK-AAC encoder], [disabled])


dnl
dnl A52/AC3 decoder plugin
dnl
AC_ARG_ENABLE([a52],
  AS_HELP_STRING([--enable-a52], [A/52 support with liba52 (default enabled)]))
if test "${enable_a52}" != "no"
then
  AC_ARG_WITH([a52],
    AS_HELP_STRING([--with-a52=PATH], [a52 headers and libraries]))
    if test -z "${with_a52}"
    then
      LDFLAGS_test=""
      CPPFLAGS_test=""
    else
      LDFLAGS_test="-L${with_a52}/lib"
      CPPFLAGS_test="-I${with_a52}/include"
    fi
    VLC_SAVE_FLAGS
    CPPFLAGS="${CPPFLAGS} ${CPPFLAGS_test} ${CPPFLAGS_a52}"
    LDFLAGS="${LDFLAGS} ${LDFLAGS_test} ${LIBS_a52}"
    AC_CHECK_HEADERS([a52dec/a52.h], [
      AC_CHECK_LIB([a52], [a52_free], [
        VLC_ADD_PLUGIN([a52])
        VLC_ADD_LIBS([a52],[${LDFLAGS_test} -la52 $LIBM])
        VLC_ADD_CPPFLAGS([a52],[${CPPFLAGS_test}])
      ],[
        AC_MSG_ERROR([Could not find liba52 on your system: you may get it from http://liba52.sf.net/. Alternatively you can use --disable-a52 to disable the a52 plugin.])
        ], [$LIBM])
    ],[
      AC_MSG_ERROR([Could not find liba52 on your system: you may get it from http://liba52.sf.net/. Alternatively you can use --disable-a52 to disable the a52 plugin.])
    ])
    VLC_RESTORE_FLAGS
fi

dnl
dnl DTS Coherent Acoustics decoder plugin
dnl
PKG_ENABLE_MODULES_VLC([DCA], [dca], [libdca >= 0.0.5], [DTS Coherent Acoustics support with libdca], [auto])

dnl
dnl  Flac plugin
dnl
PKG_ENABLE_MODULES_VLC([FLAC], [], [flac], [libflac decoder/encoder support], [auto])

dnl
dnl  Libmpeg2 plugin
dnl
PKG_ENABLE_MODULES_VLC([LIBMPEG2], [], [libmpeg2 > 0.3.2], [libmpeg2 decoder support], [auto])

dnl
dnl  Vorbis plugin
dnl
PKG_ENABLE_MODULES_VLC([VORBIS], [], [ogg vorbis >= 1.1 vorbisenc >= 1.1], [Vorbis decoder and encoder], [auto])

dnl
dnl  Tremor plugin
dnl
AC_ARG_ENABLE([tremor],
  AS_HELP_STRING([--enable-tremor], [Tremor decoder support (default disabled)]))
if test "${enable_tremor}" = "yes"
then
  AC_CHECK_HEADERS([tremor/ivorbiscodec.h], [
    VLC_ADD_PLUGIN([tremor])
   ],[])
fi

dnl
dnl  Speex plugins
dnl
PKG_ENABLE_MODULES_VLC([SPEEX], [], [ogg speex >= 1.0.5], [Speex support], [auto])
have_speexdsp="no"
AS_IF([test "${enable_speex}" != "no"], [
  PKG_CHECK_MODULES([SPEEXDSP], [speexdsp], [
    have_speexdsp="yes"
  ], [
    AC_MSG_WARN([${SPEEXDSP_PKG_ERRORS}.])
  ])
])
AM_CONDITIONAL([HAVE_SPEEXDSP], [test "$have_speexdsp" = "yes"])

dnl
dnl  Opus plugin
dnl
PKG_ENABLE_MODULES_VLC([OPUS], [], [ogg opus >= 1.0.3], [Opus support], [auto])

dnl
dnl  Ambisonic channel mixer and binauralizer plugin
dnl
PKG_ENABLE_MODULES_VLC([SPATIALAUDIO], [], [spatialaudio], [Ambisonic channel mixer and binauralizer], [auto])

dnl
dnl  theora decoder plugin
dnl
PKG_ENABLE_MODULES_VLC([THEORA], [], [ogg theoradec >= 1.0 theoraenc], [experimental theora codec], [auto])

dnl
dnl  OggSpots decoder plugin
dnl
PKG_ENABLE_MODULES_VLC([OGGSPOTS], [], [ogg], [experimental OggSpots codec], [auto])

dnl
dnl  Daala decoder plugin
dnl
PKG_ENABLE_MODULES_VLC([DAALA], [], [daaladec daalaenc], [experimental daala codec], [disabled])

dnl
dnl  schroedinger decoder plugin (for dirac format video)
dnl
PKG_ENABLE_MODULES_VLC([SCHROEDINGER], [], [schroedinger-1.0 >= 1.0.10], [dirac decoder and encoder using schroedinger], [auto])

dnl
dnl  PNG decoder module
dnl
AC_ARG_ENABLE([png],
  AS_HELP_STRING([--enable-png], [PNG support (default enabled)]))
AS_IF([test "${enable_png}" != "no"], [
AC_CHECK_HEADERS([png.h], [
  VLC_SAVE_FLAGS
  LDFLAGS="${LDFLAGS} -lz $LIBM"
  AC_CHECK_LIB([png], [png_set_alpha_mode], [
    VLC_ADD_PLUGIN([png])
  ], [], [-lz $LIBM])
  VLC_RESTORE_FLAGS
  ])
])

dnl
dnl  JPEG decoder module
dnl
AC_ARG_ENABLE([jpeg],
  AS_HELP_STRING([--enable-jpeg], [JPEG support (default enabled)]))
AS_IF([test "${enable_jpeg}" != "no"], [
AC_CHECK_HEADERS([jpeglib.h], [
  VLC_ADD_PLUGIN([jpeg])
  ])
])

dnl
dnl  BPG decoder module
dnl
AC_ARG_ENABLE([bpg],
  AS_HELP_STRING([--enable-bpg], [BPG support (default disabled)]))
AS_IF([test "${enable_bpg}" != "no"], [
AC_CHECK_HEADERS([libbpg.h], [
  VLC_ADD_PLUGIN([bpg])
  ])
])

dnl
dnl H262 encoder plugin (lib262)
dnl
AC_ARG_ENABLE([x262],
  AS_HELP_STRING([--enable-x262], [H262 encoding support with static libx262 (default disabled)]))
if test "${enable_x262}" != "no"; then
    PKG_CHECK_MODULES(X262, x262, [
        VLC_ADD_PLUGIN([x262])
        VLC_ADD_LDFLAGS([x262],[${X262_LIBS}])
        AS_IF([test -n "${ac_cv_ld_bsymbolic}"], [
          VLC_ADD_LIBS([x262],[${av_cv_ld_bsymbolic}])
        ])
        VLC_ADD_CFLAGS([x262],[${X262_CFLAGS}])
        if echo ${X262_LIBS} |grep -q 'pthreadGC2'; then
          VLC_ADD_CFLAGS([x262], [-DPTW32_STATIC_LIB])
        fi
    ], [
       if test "${enable_x262}" = "yes"; then
        AC_MSG_ERROR([x262 module doesn't work without staticly compiled libx262.a])
       fi
    ])
fi

dnl x265 encoder
PKG_ENABLE_MODULES_VLC([X265],, [x265], [HEVC/H.265 encoder], [auto])

dnl
dnl H264 encoder plugin (using libx264)
dnl
AC_ARG_ENABLE([x264],
  AS_HELP_STRING([--enable-x264], [H264 encoding support with libx264 (default enabled)]))
if test "${enable_x264}" != "no"; then
      PKG_CHECK_MODULES(X264,x264 >= 0.148, [
        VLC_ADD_PLUGIN([x264])
        VLC_ADD_LIBS([x264],[${X264_LIBS}])

        AS_IF([test -n "${ac_cv_ld_bsymbolic}"], [
          VLC_ADD_LDFLAGS([x264],[${ac_cv_ld_bsymbolic}])
        ])

        VLC_ADD_CFLAGS([x264],[${X264_CFLAGS}])
        if echo ${X264_LIBS} |grep -q 'pthreadGC2'; then
          VLC_ADD_CFLAGS([x264], [-DPTW32_STATIC_LIB])
        fi
      ],[
        if test "${enable_x264}" = "yes"; then
            AC_MSG_ERROR([${X264_PKG_ERRORS}: you may get it from http://www.videolan.org/x264.html])
          fi
      ])
fi

dnl
dnl H264 encoder plugin (libx264 > 153)
dnl
AC_ARG_ENABLE([x26410b],
  AS_HELP_STRING([--enable-x26410b], [H264 10-bit encoding support with libx264 (default enabled)]))
if test "${enable_x26410b}" != "no"; then
      PKG_CHECK_MODULES(X26410b,x264 >= 0.153, [
        VLC_ADD_PLUGIN([x26410b])
        VLC_ADD_LIBS([x26410b],[${X264_LIBS}])

        AS_IF([test -n "${ac_cv_ld_bsymbolic}"], [
          VLC_ADD_LDFLAGS([x26410b],[${ac_cv_ld_bsymbolic}])
        ])

        VLC_ADD_CFLAGS([x26410b],[${X264_CFLAGS}])
        if echo ${X264_LIBS} |grep -q 'pthreadGC2'; then
          VLC_ADD_CFLAGS([x26410b], [-DPTW32_STATIC_LIB])
        fi
      ],[
        if test "${enable_x26410b}" = "yes"; then
            AC_MSG_ERROR([${X264_PKG_ERRORS}: you may get it from http://www.videolan.org/x264.html])
          fi
      ])
fi


dnl
dnl Intel QuickSync (aka MediaSDK) H264/H262 encoder
dnl
PKG_ENABLE_MODULES_VLC([MFX], [qsv], [libmfx], [Intel QuickSync MPEG4-Part10/MPEG2 (aka H.264/H.262) encoder], [auto])

dnl
dnl libfluidsynth (MIDI synthetizer) plugin
dnl
PKG_ENABLE_MODULES_VLC([FLUIDSYNTH], [], [fluidsynth >= 1.1.2], [MIDI synthetiser with libfluidsynth], [auto])
PKG_ENABLE_MODULES_VLC([FLUIDLITE], [fluidsynth], [fluidlite], [MIDI synthetiser with libfluidsynth], [auto])
AC_CHECK_HEADERS([fluidlite.h], [AC_SUBST([FLUIDSYNTH_LIBS], ${FLUIDLITE_LIBS})])

dnl
dnl Teletext Modules
dnl vbi decoder plugin (using libzbvi)
dnl telx module
dnl uncompatible
dnl
AC_ARG_ENABLE([zvbi],
  AS_HELP_STRING([--enable-zvbi],[VBI (inc. Teletext) decoding support with
                  libzvbi (default enabled)]))
AC_ARG_ENABLE([telx],
  AS_HELP_STRING([--enable-telx],[Teletext decoding module (conflicting with
                  zvbi) (default enabled if zvbi is absent)]))

AS_IF( [test "${enable_zvbi}" != "no"],[
    PKG_CHECK_MODULES(ZVBI, [zvbi-0.2 >= 0.2.28],
        [
          VLC_ADD_PLUGIN([zvbi])
          AC_DEFINE(ZVBI_COMPILED, 1, [Define if the zvbi module is built])
          have_zvbi="yes"
          AS_IF( [test "${enable_telx}" = "yes"],[
                  AC_MSG_WARN([The zvbi and telx modules are incompatible. Using zvbi.])
                  ])
        ],[
          AC_MSG_WARN([${ZVBI_PKG_ERRORS}. Enabling the telx module instead.])
        ])
    ])
AM_CONDITIONAL([HAVE_ZVBI], [test "x${have_zvbi}" = x"yes"])

AS_IF( [test "${enable_telx}" != "no" ],[
  VLC_ADD_PLUGIN([telx])
  have_telx="yes"
  AC_DEFINE(TELX_COMPILED, 1, [Define if the telx module is built])
  ])
AM_CONDITIONAL([HAVE_TELX], [test "x${have_telx}" = x"yes"])

dnl
dnl ARIB subtitles rendering module
dnl
AC_ARG_ENABLE([aribsub],
  AS_HELP_STRING([--enable-aribsub],[ARIB Subtitles support (default enabled)]))
AS_IF( [test "${enable_aribsub}" != "no" ],[
  PKG_CHECK_MODULES(ARIBB24, [aribb24 >= 1.0.1], [
      have_aribb24="yes"
      VLC_ADD_PLUGIN([aribsub])
      VLC_ADD_LIBS([aribsub],[-laribb24])
      AC_DEFINE(HAVE_ARIBB24, 1, [Define if libaribb24 is available.])
    ],[
      AC_MSG_WARN(Library [aribb24] needed for [aribsub] was not found)
      have_aribb24="no"
  ])
])
AM_CONDITIONAL([HAVE_ARIBB24], [test x"${have_aribb24}" = x"yes"])

dnl
dnl ARIB B25
dnl
PKG_ENABLE_MODULES_VLC([ARIBB25], [aribcam], [aribb25 >= 0.2.6], [ARIB STD-B25], [auto])

dnl
dnl  kate decoder plugin
dnl
PKG_ENABLE_MODULES_VLC([KATE], [], [kate >= 0.3.0], [kate codec], [auto])

dnl
dnl  tiger rendering for kate decoder plugin
dnl
AC_ARG_ENABLE([tiger],
AS_HELP_STRING([--enable-tiger], [Tiger rendering library for Kate streams (default auto)]))
AS_IF([test "${enable_tiger}" != "no"], [
  PKG_CHECK_MODULES(TIGER,[tiger >= 0.3.1], [
      AC_DEFINE(HAVE_TIGER, 1, [Define if libtiger is available.])],[
        AS_IF([test -n "${enable_tiger}"], [
          AC_MSG_ERROR([${TIGER_PKG_ERRORS}.])
        ])
  ])
])

dnl
dnl  check if we can build the css selector engine
dnl
AC_ARG_ENABLE([css],
AS_HELP_STRING([--enable-css], [CSS selector engine (default auto)]))
AS_IF([test "${enable_css}" != "no"], [
  with_css="yes"
  AC_DEFINE(HAVE_CSS, 1, [Define if CSS engine is built])
])
AM_CONDITIONAL([ENABLE_CSS], [test "${with_css}" = "yes"])


dnl
dnl  Video plugins
dnl

EXTEND_HELP_STRING([Video plugins:])

dnl
dnl  libplacebo support
dnl
AC_ARG_ENABLE([libplacebo],
  AS_HELP_STRING([--disable-libplacebo],
      [disable libplacebo support (default auto)]))

AS_IF([test "$enable_libplacebo" != "no"], [
  PKG_CHECK_MODULES([LIBPLACEBO], [libplacebo >= 0.5], [
    AC_DEFINE([HAVE_LIBPLACEBO], [1], [Define to 1 if libplacebo is enabled.])
    PKG_CHECK_MODULES([LIBPLACEBO], [libplacebo >= 1.7],
      [have_libplacebo_vk="yes"], [have_libplacebo_vk="no"])
  ], [
    AS_IF([test -n "${enable_libplacebo}"], [
      AC_MSG_ERROR([${LIBPLACEBO_PKG_ERRORS}.])
    ])
    enable_libplacebo="no"
  ])
])
AM_CONDITIONAL([HAVE_LIBPLACEBO], [test "$enable_libplacebo" != "no"])

dnl
dnl  OpenGL
dnl
PKG_CHECK_MODULES([EGL], [egl], [
  have_egl="yes"
], [
  have_egl="no"
])
AM_CONDITIONAL([HAVE_EGL], [test "${have_egl}" = "yes"])

have_gl="no"
PKG_CHECK_MODULES([GL], [gl], [
  have_gl="yes"
], [
  AC_MSG_CHECKING([for OpenGL])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#ifdef _WIN32
# include <GL/glew.h>
#endif
#include <GL/gl.h>
]], [
    [int t0 = GL_TEXTURE0;]])
  ], [
    GL_CFLAGS=""
    have_gl="yes"
    AS_IF([test "${SYS}" != "mingw32"], [
      GL_LIBS="-lGL"
    ], [
      GL_LIBS="-lopengl32"
    ])
  ])
  AC_MSG_RESULT([${have_gl}])
])
AM_CONDITIONAL([HAVE_GL], [test "${have_gl}" = "yes"])
AS_IF([test "${have_gl}" = "yes"], [
  AC_DEFINE([HAVE_GL], 1, [Defined if having OpenGL])
])

dnl OpenGL ES 2: depends on EGL 1.1
have_gles2="no"
AC_ARG_ENABLE(gles2,
  [AS_HELP_STRING([--enable-gles2],
      [enable GLES2 support (default disabled)])])
AS_IF([test "$enable_gles2" = "yes"], [
  PKG_CHECK_MODULES([GLES2], [glesv2], [have_gles2="yes"], [])
  ])
AM_CONDITIONAL([HAVE_GLES2], [test "${have_gles2}" = "yes"])

dnl
dnl Vulkan
dnl
AC_ARG_ENABLE(vulkan,
  [AS_HELP_STRING([--disable-vulkan],
      [disable Vulkan support (default auto)])])

AS_IF([test "$enable_vulkan" != "no"], [
  AS_IF([test "$have_libplacebo_vk" = "yes"], [
    PKG_CHECK_MODULES([VULKAN], [vulkan >= 1.0.26], [
      AC_DEFINE([HAVE_VULKAN], [1], [Define to 1 if vulkan is enabled.])
    ], [
      AS_IF([test -n "${enable_vulkan}"], [
        AC_MSG_ERROR([${VULKAN_PKG_ERRORS}.])
      ])
      enable_vulkan="no"
    ])
  ], [
    AS_IF([test -n "${enable_vulkan}"], [
      AC_MSG_ERROR([Vulkan support requires libplacebo 1.7 as well. Do not use --disable-libplacebo.])
    ])
    enable_vulkan="no"
  ])
])
AM_CONDITIONAL(HAVE_VULKAN, [test "$enable_vulkan" != "no"])

dnl
dnl  Xlib
dnl

AC_PATH_XTRA()
AC_CHECK_HEADERS([X11/Xlib.h])

dnl
dnl  X C Bindings modules
dnl
AC_ARG_ENABLE([xcb],
  AS_HELP_STRING([--enable-xcb], [X11 support with XCB (default enabled)]),, [
  AS_IF([test "${SYS}" != "mingw32" -a "${SYS}" != "darwin"], [
    enable_xcb="yes"
  ], [
    enable_xcb="no"
  ])
])

have_xcb="no"
have_xkbcommon_x11="no"
have_xcb_keysyms="no"
AS_IF([test "${enable_xcb}" != "no"], [
  xcb_err=""

  dnl libxcb
  PKG_CHECK_MODULES([XCB], [xcb >= 1.6],, [
    xcb_err="$xcb_err ${XCB_PKG_ERRORS}."])
  PKG_CHECK_MODULES([XCB_COMPOSITE], [xcb-composite],, [
    xcb_err="$xcb_err ${XCB_COMPOSITE_PKG_ERRORS}."])
  PKG_CHECK_MODULES([XCB_RANDR], [xcb-randr >= 1.3],, [
    xcb_err="$xcb_err ${XCB_RANDR_PKG_ERRORS}."])
  PKG_CHECK_MODULES([XCB_RENDER], [xcb-render],, [
    xcb_err="$xcb_err ${XCB_RENDER_PKG_ERRORS}."])
  PKG_CHECK_MODULES([XCB_SHM], [xcb-shm >= 1.9.2],, [
    xcb_err="$xcb_err ${XCB_SHM_PKG_ERRORS}."])
  PKG_CHECK_MODULES([XCB_XKB], [xcb-xkb],, [
    xcb_err="$xcb_err ${XCB_XKB_PKG_ERRORS}."])

  dnl xproto
  PKG_CHECK_MODULES(XPROTO, [xproto],, [
    xcb_err="$xcb_err ${XPROTO_PKG_ERRORS}."])

  AS_IF([test -n "$xcb_err"], [
    AC_MSG_ERROR([$xcb_err Pass --disable-xcb to skip X11 support.])
  ])

  dnl xkbcommon
  PKG_CHECK_MODULES([XKBCOMMON_X11], [xkbcommon-x11], [
    have_xkbcommon_x11="yes"
  ], [
    AC_MSG_WARN([${XKBCOMMON_X11_PKG_ERRORS}. Hotkeys are disabled.])
  ])

  dnl xcb-utils
  PKG_CHECK_MODULES([XCB_KEYSYMS], [xcb-keysyms >= 0.3.4], [
    have_xcb_keysyms="yes"
  ], [
    AC_MSG_WARN([${XCB_KEYSYMS_PKG_ERRORS}. Global hotkeys are disabled.])
  ])

  have_xcb="yes"
])
AM_CONDITIONAL([HAVE_XCB], [test "${have_xcb}" = "yes"])
AM_CONDITIONAL([HAVE_XKBCOMMON_X11], [test "${have_xkbcommon_x11}" = "yes"])
AM_CONDITIONAL([HAVE_XCB_KEYSYMS], [test "${have_xcb_keysyms}" = "yes"])


dnl
dnl VDPAU needs X11
dnl
AC_ARG_ENABLE([vdpau],
  [AS_HELP_STRING([--enable-vdpau], [VDPAU hardware support (default auto)])])
have_vdpau="no"
AS_IF([test "${enable_vdpau}" != "no"], [
  PKG_CHECK_MODULES([VDPAU], [vdpau >= 0.6], [
    have_vdpau="yes"
    AS_IF([test "${no_x}" = "yes"], [
      AC_MSG_ERROR([VDPAU requires Xlib (X11).])
    ])
  ], [
    AS_IF([test -n "${enable_vdpau}"], [
      AC_MSG_ERROR([${VDPAU_PKG_ERRORS}.])
    ])
  ])
])
AM_CONDITIONAL([HAVE_VDPAU], [test "${have_vdpau}" = "yes"])

have_avcodec_vdpau="no"
AS_IF([test "${have_vdpau}" = "yes" -a "${have_avcodec}" = "yes"], [
  case "${avfork}" in
    libav) av_vdpau_ver="56.10.0" ;;
    ffmpeg) av_vdpau_ver="56.19.100" ;;
  esac
  PKG_CHECK_EXISTS([libavcodec >= ${av_vdpau_ver}], [
    have_avcodec_vdpau="yes"
    AC_MSG_NOTICE([VDPAU decoding acceleration activated])
  ], [
    AS_IF([test -n "${enable_vdpau}"], [
      AC_MSG_ERROR([libavcodec >= 56.10.0 is required for VDPAU decoding.])
    ], [
      AC_MSG_WARN([libavcodec >= 56.10.0 are required for VDPAU decoding.])
    ])
  ])
])
AM_CONDITIONAL([HAVE_AVCODEC_VDPAU], [test "${have_avcodec_vdpau}" = "yes"])


dnl
dnl  Wayland
dnl
AC_ARG_ENABLE([wayland],
  AS_HELP_STRING([--enable-wayland], [Wayland support (default auto)]))

have_wayland="no"
AS_IF([test "${enable_wayland}" != "no"], [
  wl_err=""

  dnl wayland
  PKG_CHECK_MODULES([WAYLAND_CLIENT], [wayland-client >= 1.5.91],, [
    wl_err="$wl_err ${WAYLAND_CLIENT_PKG_ERRORS}."
  ])
  PKG_CHECK_MODULES([WAYLAND_CURSOR], [wayland-cursor],, [
    wl_err="$wl_err ${WAYLAND_CURSOR_PKG_ERRORS}."
  ])
  PKG_CHECK_MODULES([WAYLAND_EGL], [wayland-egl],, [
    wl_err="$wl_err ${WAYLAND_EGL_PKG_ERRORS}."
  ])
  PKG_CHECK_MODULES([WAYLAND_SCANNER], [wayland-scanner >= 1.15], [
    WAYLAND_SCANNER="$(${PKG_CONFIG} wayland-scanner --variable wayland_scanner)"
  ], [
    wl_err="$wl_err ${WAYLAND_SCANNER_PKG_ERRORS}."
  ])

  dnl wayland-protocols
  PKG_CHECK_MODULES([WAYLAND_PROTOCOLS], [wayland-protocols >= 1.15], [
    WAYLAND_PROTOCOLS="$(${PKG_CONFIG} wayland-protocols --variable pkgdatadir)"
  ], [
    wl_err="$wl_err ${WAYLAND_PROTOCOLS_PKG_ERRORS}."
  ])

  AS_IF([test -n "$wl_err"], [
    AS_IF([test -n "$enable_wayland"], [
      AC_MSG_ERROR([$wl_err])
    ], [
      AC_MSG_WARN([$wl_err])
    ])
  ], [
    have_wayland="yes"
  ])

  dnl xkbcommon
  PKG_CHECK_MODULES([XKBCOMMON], [xkbcommon], [
    have_xkbcommon="yes"
  ], [
    AC_MSG_WARN([${XKBCOMMON_PKG_ERRORS}. Hotkeys are disabled.])
  ])
])
AM_CONDITIONAL([HAVE_WAYLAND], [test "${have_wayland}" = "yes"])
AC_SUBST([WAYLAND_PROTOCOLS])
AC_SUBST([WAYLAND_SCANNER])
AM_CONDITIONAL([HAVE_XKBCOMMON], [test "${have_xkbcommon}" = "yes"])


dnl
dnl  SDL image decoder module
dnl
AC_ARG_ENABLE([sdl-image],
  AS_HELP_STRING([--enable-sdl-image], [SDL image support (default enabled)]))
AS_IF([test "${enable_sdl_image}" != "no"],[
  PKG_CHECK_MODULES(SDL_IMAGE, [SDL_image >= 1.2.10], [
    VLC_ADD_PLUGIN([sdl_image])
  ], [
    AC_MSG_WARN([${SDL_IMAGE_PKG_ERRORS}. You should install it alongside your SDL package.])
  ])
])

dnl
dnl  freetype module
dnl
AC_ARG_ENABLE([freetype],
  AS_HELP_STRING([--enable-freetype],   [freetype support   (default auto)]))
AC_ARG_ENABLE([fribidi],
  AS_HELP_STRING([--enable-fribidi],    [fribidi support    (default auto)]))
AC_ARG_ENABLE([harfbuzz],
  AS_HELP_STRING([--enable-harfbuzz],   [harfbuzz support   (default auto)]))
AC_ARG_ENABLE([fontconfig],
  AS_HELP_STRING([--enable-fontconfig], [fontconfig support (default auto)]))

AC_ARG_WITH([default-font],
    AS_HELP_STRING([--with-default-font=PATH],
        [Path to the default font]),
        [AC_DEFINE_UNQUOTED([DEFAULT_FONT_FILE],
            "$withval", [Default font])])
AC_ARG_WITH([default-monospace-font],
    AS_HELP_STRING([--with-default-monospace-font=PATH],
        [Path to the default font]),
        [AC_DEFINE_UNQUOTED([DEFAULT_MONOSPACE_FONT_FILE],
            "$withval", [Default monospace font])])

AC_ARG_WITH([default-font-family],
    AS_HELP_STRING([--with-default-font-family=NAME],
        [Name of the default font family]),
        [AC_DEFINE_UNQUOTED([DEFAULT_FAMILY],
            "$withval", [Default font family])])
AC_ARG_WITH([default-monospace-font-family],
    AS_HELP_STRING([--with-default-monospace-font-family=NAME],
        [Name of the default font family]),
        [AC_DEFINE_UNQUOTED([DEFAULT_MONOSPACE_FAMILY],
            "$withval", [Default monospace font family])])

have_freetype="no"
have_fontconfig="no"
have_fribidi="no"
have_harfbuzz="no"

if test "${enable_freetype}" != "no"; then
   PKG_CHECK_MODULES(FREETYPE, freetype2, [
      have_freetype="yes"
      VLC_ADD_CPPFLAGS([skins2],[${FREETYPE_CFLAGS}])
      VLC_ADD_LIBS([skins2],[${FREETYPE_LIBS}])

    dnl fontconfig support
    AS_IF([test "${enable_fontconfig}" != "no" -a "${SYS}" != "mingw32" -a "${SYS}" != "darwin"], [
      PKG_CHECK_MODULES(FONTCONFIG, [fontconfig >= 2.11], [
        have_fontconfig="yes"
      ], [
        AC_MSG_WARN([${FONTCONFIG_PKG_ERRORS}. Styles will be disabled in FreeType.])
      ])
    ])

      dnl fribidi support
      if test "${enable_fribidi}" != "no"; then
        PKG_CHECK_MODULES(FRIBIDI, fribidi, [
          have_fribidi="yes"
          VLC_ADD_CPPFLAGS([skins2], [${FRIBIDI_CFLAGS} -DHAVE_FRIBIDI])
          VLC_ADD_LIBS([skins2], [${FRIBIDI_LIBS}])
        ],[AC_MSG_WARN([${FRIBIDI_PKG_ERRORS}. Bidirectional text and complex scripts (Arabic, Farsi, Thai...) will be disabled in FreeType.])])
      fi

      dnl harfbuzz support
      if test "${have_fribidi}" != "no"; then
        if test "${enable_harfbuzz}" != "no"; then
          PKG_CHECK_MODULES(HARFBUZZ, harfbuzz, [
            have_harfbuzz="yes"
            VLC_ADD_CPPFLAGS([skins2], [${HARFBUZZ_CFLAGS} -DHAVE_HARFBUZZ])
            VLC_ADD_LIBS([skins2], [${HARFBUZZ_LIBS}])
          ],[AC_MSG_WARN([${HARFBUZZ_PKG_ERRORS}. Support for complex scripts (Arabic, Farsi, Thai...) will be disabled in FreeType.])])
        fi
      fi
  ],[
  AS_IF([test -n "${enable_freetype}"],[
    AC_MSG_ERROR([${FREETYPE_PKG_ERRORS}. Install FreeType2 development or configure with --disable-freetype.])
    ])
  ])
fi

AM_CONDITIONAL([HAVE_FREETYPE], [test "${have_freetype}" = "yes"])
AM_CONDITIONAL([HAVE_FONTCONFIG], [test "${have_fontconfig}" = "yes"])
AM_CONDITIONAL([HAVE_FRIBIDI], [test "${have_fribidi}" = "yes"])
AM_CONDITIONAL([HAVE_HARFBUZZ], [test "${have_harfbuzz}" = "yes"])

dnl
dnl libass subtitle rendering module
dnl
AC_ARG_ENABLE([libass],
  AS_HELP_STRING([--enable-libass], [Subtitle support using libass (default enabled)]))
AS_IF( [test "${enable_libass}" != "no"], [
  PKG_CHECK_MODULES(LIBASS, [libass >= 0.9.8],
      [
        VLC_ADD_PLUGIN([libass])

        if test "${have_fontconfig}" != "no"; then
            VLC_ADD_CPPFLAGS([libass],[-DHAVE_FONTCONFIG])
            VLC_ADD_LIBS([libass],[${FONTCONFIG_LIBS}])
        fi
      ],[
        AC_MSG_WARN([${LIBASS_PKG_ERRORS}.])
      ])
  ])

dnl
dnl SAPI (text to Speech renderer for Windows)
dnl
AC_LANG_PUSH([C++])
AC_CHECK_TYPES([ISpObjectToken],[
              have_sapi="yes"
            ],[AC_MSG_WARN([sapi.h not found. Text to Speech renderer for Windows disabled])],
              [#include <windows.h>
               #include <sapi.h>
               #include <sphelper.h>])
AM_CONDITIONAL([HAVE_SAPI], [test "${have_sapi}" = "yes"])
AC_LANG_POP([C++])

dnl
dnl  SVG module (text rendering and image decoding)
dnl
PKG_ENABLE_MODULES_VLC([SVG], [], [librsvg-2.0 >= 2.9.0], [SVG rendering library],[auto])
PKG_ENABLE_MODULES_VLC([SVGDEC], [], [librsvg-2.0 >= 2.9.0 cairo >= 1.13.1], [SVG image decoder library],[auto])

dnl
dnl  Windows DirectX module
dnl

AC_ARG_ENABLE([directx],
  AS_HELP_STRING([--enable-directx],
    [Microsoft DirectX support (default enabled on Windows)]),, [
  AS_IF([test "${SYS}" = "mingw32"], [
    enable_directx="yes"
  ], [
    enable_directx="no"
  ])
])
AS_IF([test "${enable_directx}" != "no"], [
  dnl OpenGL
  AC_CHECK_HEADERS([GL/glew.h], [
    AC_CHECK_HEADERS([GL/wglew.h], [
        VLC_ADD_PLUGIN([glwin32 wgl])
      ],, [
        #include <windows.h>
        #include <GL/glew.h>
      ])],, [
      #include <windows.h>
   ])

  dnl Direct3D11
  AC_CHECK_HEADERS([d3d11.h], [
    VLC_ADD_PLUGIN([direct3d11 direct3d11_filters])
  ])

  dnl Direct3D9
  AC_CHECK_HEADERS([d3d9.h], [
    VLC_ADD_PLUGIN([direct3d9])
    AC_CHECK_HEADERS([d3dx9effect.h])

    AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
        [[#define COBJMACROS
          #include <d3d9.h>
          #include <dxva2api.h>
          #ifndef IDirectXVideoProcessorService_GetVideoProcessorDeviceGuids
          # error dxva2 too old
          #endif
        ]],[[;]])
      ],[
        VLC_ADD_PLUGIN([direct3d9_filters])
      ],
      [AC_MSG_WARN([Could not find required IDirectXVideoDecoder in dxva2api.h])])

  ])
])

dnl
dnl  Linux framebuffer module
dnl
AC_CHECK_HEADER([linux/fb.h], [
  VLC_ADD_PLUGIN([fb])
])

dnl
dnl  Linux kernel mode setting module
dnl
PKG_ENABLE_MODULES_VLC([KMS], [], [libdrm >= 2.4.83], [Linux kernel mode setting output], [auto])

dnl
dnl  libcaca plugin
dnl
PKG_ENABLE_MODULES_VLC([CACA], [], [caca >= 0.99.beta14], [libcaca output],[auto])

dnl
dnl  OS/2 KVA plugin
dnl
AC_ARG_ENABLE([kva],
  AS_HELP_STRING([--enable-kva],
    [support the K Video Accelerator KVA (default enabled on OS/2)]),, [
  AS_IF([test "$SYS" = "os2"], [
    enable_kva="yes"
  ])
])
have_kva="no"
KVA_LIBS=""
AS_IF([test "$enable_kva" != "no"], [
  AC_CHECK_HEADERS([kva.h], [
    have_kva="yes"
    AC_CHECK_LIB([kva], [main], [
      KVA_LIBS="-lkva"
    ])
  ])
])
AC_SUBST([KVA_LIBS])
AM_CONDITIONAL([HAVE_KVA], [test "${have_kva}" = "yes"])

dnl
dnl MMAL plugin
dnl
AC_ARG_ENABLE([mmal],
  AS_HELP_STRING([--enable-mmal],
    [Multi-Media Abstraction Layer (MMAL) hardware plugin (default enable)]))
if test "${enable_mmal}" != "no"; then
  VLC_SAVE_FLAGS
  LDFLAGS="${LDFLAGS} -L/opt/vc/lib -lvchostif"
  CPPFLAGS="${CPPFLAGS} -isystem /opt/vc/include -isystem /opt/vc/include/interface/vcos/pthreads -isystem /opt/vc/include/interface/vmcs_host/linux"
  AC_CHECK_HEADERS([interface/mmal/mmal.h],
    [ AC_CHECK_LIB([bcm_host], [vc_tv_unregister_callback_full], [
        have_mmal="yes"
        VLC_ADD_PLUGIN([mmal])
        VLC_ADD_LDFLAGS([mmal],[ -L/opt/vc/lib ])
        VLC_ADD_CFLAGS([mmal],[ -isystem /opt/vc/include -isystem /opt/vc/include/interface/vcos/pthreads -isystem /opt/vc/include/interface/vmcs_host/linux ])
        VLC_ADD_LIBS([mmal],[ -lbcm_host -lmmal -lmmal_core -lmmal_components -lmmal_util -lvchostif -lvcsm -lvchiq_arm ]) ], [
          AS_IF([test "${enable_mmal}" = "yes"],
            [ AC_MSG_ERROR([Cannot find bcm library...]) ],
            [ AC_MSG_WARN([Cannot find bcm library...]) ])
          ],
        [])
    ] , [ AS_IF([test "${enable_mmal}" = "yes"],
      [ AC_MSG_ERROR([Cannot find development headers for mmal...]) ],
      [ AC_MSG_WARN([Cannot find development headers for mmal...]) ]) ])
  VLC_RESTORE_FLAGS
fi
AM_CONDITIONAL([HAVE_MMAL], [test "${have_mmal}" = "yes"])

dnl
dnl  Audio plugins
dnl

EXTEND_HELP_STRING([Audio plugins:])

dnl
dnl  Pulseaudio module
dnl
AC_ARG_ENABLE([pulse],
  AS_HELP_STRING([--enable-pulse],
    [use the PulseAudio client library (default auto)]))
have_pulse="no"
AS_IF([test "${enable_pulse}" != "no"], [
  PKG_CHECK_MODULES([PULSE], [libpulse >= 1.0], [
    have_pulse="yes"
  ], [
    AS_IF([test "x${enable_pulse}" != "x"], [
      AC_MSG_ERROR([$PULSE_PKG_ERRORS. PulseAudio 1.0 or later required.])
    ])
  ])
])
AM_CONDITIONAL([HAVE_PULSE], [test "${have_pulse}" = "yes"])

dnl
dnl  ALSA module
dnl
AC_ARG_ENABLE([alsa],
  AS_HELP_STRING([--enable-alsa],
    [support the Advanced Linux Sound Architecture (default auto)]),, [
  AS_IF([test "$SYS" = "linux" -a "${have_pulse}" = "no"], [
    enable_alsa="yes"
  ])
])
have_alsa="no"
AS_IF([test "${enable_alsa}" != "no"], [
  PKG_CHECK_MODULES([ALSA], [alsa >= 1.0.24], [
    have_alsa="yes"
  ], [
    AS_IF([test "x${enable_alsa}" != "x"], [
      AC_MSG_ERROR([$ALSA_PKG_ERRORS. alsa-lib 1.0.24 or later required. Pass --disable-alsa to ignore this error.])
    ])
  ])
])
AM_CONDITIONAL([HAVE_ALSA], [test "${have_alsa}" = "yes"])

dnl
dnl  Open Sound System module
dnl
AC_ARG_ENABLE([oss],
  AS_HELP_STRING([--enable-oss],
    [support the Open Sound System OSS (default enabled on FreeBSD/NetBSD/DragonFlyBSD)]),, [
  AS_IF([test "$SYS" = "mingw32" -o "$SYS" = "linux" -o "$SYS" = "openbsd"], [
    enable_oss="no"
  ])
])
have_oss="no"
OSS_LIBS=""
AS_IF([test "$enable_oss" != "no"], [
  AC_CHECK_HEADERS([soundcard.h sys/soundcard.h], [
    have_oss="yes"
    AC_CHECK_LIB([ossaudio], [main], [
      OSS_LIBS="-lossaudio"
    ])
  ])
])
AC_SUBST([OSS_LIBS])
AM_CONDITIONAL([HAVE_OSS], [test "${have_oss}" = "yes"])

dnl
dnl  OpenBSD sndio module
dnl
AC_ARG_ENABLE([sndio],
  AS_HELP_STRING([--disable-sndio],
    [support the OpenBSD sndio (default auto)]),, [
  AS_IF([test "$SYS" = "openbsd"], [
    enable_sndio="yes"
  ])
])
have_sndio="no"
AS_IF([test "$enable_sndio" != "no"], [
  AC_CHECK_HEADER([sndio.h], [
    have_sndio="yes"
  ])
])
AM_CONDITIONAL([HAVE_SNDIO], [test "${have_sndio}" = "yes"])

dnl
dnl  Windows Audio Session plugin
dnl
AC_ARG_ENABLE([wasapi],
  AS_HELP_STRING([--enable-wasapi],
    [use the Windows Audio Session API (default auto)])
)
have_wasapi="no"
AS_IF([test "$enable_wasapi" != "no"], [
  AC_CHECK_HEADER([audioclient.h], [
    have_wasapi="yes"
  ], [
    AS_IF([test "x${enable_wasapi}" != "x"], [
      AC_MSG_ERROR([Windows Audio Session API not found.])
    ])
  ])
])
AM_CONDITIONAL([HAVE_WASAPI], [test "${have_wasapi}" = "yes"])

dnl
dnl  JACK modules
dnl
AC_ARG_ENABLE([jack],
  AS_HELP_STRING([--disable-jack],
    [do not use jack (default auto)]))
have_jack="no"
AS_IF([test "${enable_jack}" != "no"], [
    PKG_CHECK_MODULES(JACK, jack >= 1.9.7,
      [ have_jack=yes ],
      [
      AC_MSG_WARN([${JACK_PKG_ERRORS}, trying jack1 instead])

      PKG_CHECK_MODULES(JACK, jack >= 0.120.1 jack < 1.0,
      [ have_jack=yes ],
      [
      AS_IF([test -n "${enable_jack}"],
         [AC_MSG_ERROR([${JACK_PKG_ERRORS}.])],
         [AC_MSG_WARN([${JACK_PKG_ERRORS}.])])
      ])
    ])
])
AM_CONDITIONAL([HAVE_JACK], [test "${have_jack}" != "no"])

dnl
dnl  OpenSLES Android
dnl
AC_ARG_ENABLE([opensles],
  AS_HELP_STRING([--enable-opensles], [Android OpenSL ES audio module (default disabled)]))
if test "${HAVE_ANDROID}" = "1"; then
  if test "${enable_opensles}" = "yes"; then
      AC_CHECK_HEADERS([SLES/OpenSLES.h],
        [ VLC_ADD_PLUGIN([opensles_android]) ],
        [ AC_MSG_ERROR([cannot find OpenSLES headers])] )
  fi
fi

dnl
dnl libsamplerate plugin
dnl
PKG_ENABLE_MODULES_VLC([SAMPLERATE], [], [samplerate], [Resampler with libsamplerate], [auto])

dnl
dnl  soxr module
dnl
PKG_ENABLE_MODULES_VLC([SOXR], [], [soxr >= 0.1.2], [SoX Resampler library], [auto])

dnl
dnl  OS/2 KAI plugin
dnl
AC_ARG_ENABLE([kai],
  AS_HELP_STRING([--enable-kai],
    [support the K Audio Interface KAI (default enabled on OS/2)]),, [
  AS_IF([test "$SYS" = "os2"], [
    enable_kai="yes"
  ])
])
have_kai="no"
KAI_LIBS=""
AS_IF([test "$enable_kai" != "no"], [
  AC_CHECK_HEADERS([kai.h], [
    have_kai="yes"
    AC_CHECK_LIB([kai], [main], [
      KAI_LIBS="-lkai"
    ])
  ])
])
AC_SUBST([KAI_LIBS])
AM_CONDITIONAL([HAVE_KAI], [test "${have_kai}" = "yes"])

dnl
dnl  chromaprint audio track fingerprinter
dnl
PKG_ENABLE_MODULES_VLC([CHROMAPRINT], [stream_out_chromaprint], [libchromaprint >= 0.6.0], [Chromaprint audio fingeprinter], [auto])

dnl
dnl  Chromecast streaming support
dnl
m4_pushdef([protobuf_lite_version], 2.5.0)
AC_ARG_VAR(PROTOC, [protobuf compiler])
AC_CHECK_PROGS(PROTOC, protoc, no)
PKG_WITH_MODULES([CHROMECAST],[protobuf-lite >= protobuf_lite_version], [
    AS_IF([test "x${PROTOC}" != "xno"], [
        build_chromecast="yes"
    ], [
    AC_MSG_ERROR(protoc compiler needed for [chromecast] was not found)
    ])
], [
    AS_IF([test "${enable_chromecast}" = "yes"],
        AC_MSG_ERROR(Library [protobuf-lite >= protobuf_lite_version] needed for [chromecast] was not found),
        AC_MSG_WARN(Library [protobuf-lite >= protobuf_lite_version] needed for [chromecast] was not found)
    )
    enable_chromecast="no"
], [(Chromecast streaming support)], [auto])
AM_CONDITIONAL([BUILD_CHROMECAST], [test "${build_chromecast}" = "yes"])
m4_popdef([protobuf_lite_version])

dnl
dnl  Interface plugins
dnl

EXTEND_HELP_STRING([Interface plugins:])

dnl
dnl QT
dnl
AC_ARG_ENABLE([qt],
  AS_HELP_STRING([--disable-qt], [Qt UI support (default enabled)]),, [
  AS_IF([test "${enable_vlc}" != "no"], [enable_qt="yes"])
  AS_IF([test "${SYS}" = "darwin"], [
    enable_qt="no"
  ])
])
have_qt5_x11="no"
AS_IF([test "${enable_qt}" != "no"], [
  PKG_CHECK_MODULES([QT], [Qt5Core >= 5.11.0 Qt5Widgets Qt5Gui Qt5Quick Qt5QuickWidgets Qt5QuickControls2 Qt5Svg], [
      PKG_CHECK_MODULES([QT5_X11], [Qt5X11Extras], [
          have_qt5_x11="yes"
      ],[
          AC_MSG_WARN([Not building Qt Interface with X11 helpers.])
      ])
      QT_PATH="$(eval $PKG_CONFIG --variable=exec_prefix Qt5Core)"
      QT_HOST_PATH="$(eval $PKG_CONFIG --variable=host_bins Qt5Core)"
      QT_VERSION="$(eval $PKG_CONFIG --modversion Qt5Gui)"
      AC_PATH_PROGS(MOC, [moc-qt5 moc], moc, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
      AC_PATH_PROGS(RCC, [rcc-qt5 rcc], rcc, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
      AC_PATH_PROGS(UIC, [uic-qt5 uic], uic, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
      AC_PATH_PROGS(QMLCACHE, [qmlcachegen-qt5 qmlcachegen], no, ["${QT_HOST_PATH}" "${QT_PATH}/bin"])
      AS_IF([test "x${QMLCACHE}" = "xno"], [
          AC_MSG_WARN([qmlcachegen not found])
      ])
    ], [
      AS_IF([test -n "${enable_qt}"],[
        AC_MSG_ERROR([${QT_PKG_ERRORS}. If you want to build VLC without GUI, pass --disable-qt.])
      ],[
        AC_MSG_WARN([${QT_PKG_ERRORS}.])
      ])
      enable_qt="no"
    ])
])

AC_ARG_ENABLE([qt-qml-cache],
  AS_HELP_STRING([--enable-qt-qml-cache],
    [enable qml cache generation]))
AM_CONDITIONAL([HAVE_QMLCACHE], [test "${enable_qt_qml_cache}" = "yes" && test "x$QMLCACHE" != "xno"])

AC_ARG_ENABLE([qt-qml-debug],
  AS_HELP_STRING([--enable-qt-qml-debug], [enable qml debuger]))
AM_CONDITIONAL(QT_QML_DEBUG, [test "${enable_qt_qml_debug}" = "yes"])

AS_IF([test "${enable_qt}" != "no"], [
  ALIASES="${ALIASES} qvlc"
])
AC_SUBST([QT_VERSION])
AM_CONDITIONAL([ENABLE_QT], [test "$enable_qt" != "no"])
AM_CONDITIONAL([HAVE_QT5_X11], [test "${have_qt5_x11}" = "yes"])

dnl
dnl detect kde4-config patch (used for kde solids).
dnl
AC_ARG_VAR([KDE4_CONFIG], [kde4-config utility])
AS_IF([test "x$KDE4_CONFIG" = "x"], [
  KDE4_CONFIG="kde4-config"
])

dnl
dnl Simple test for skins2 dependency
dnl
AS_IF([test "$enable_qt" = "no"], [
  AS_IF([test "${enable_skins2}" = "yes"], [
    AC_MSG_ERROR([The skins2 module depends on the Qt interface. Without it you will not be able to open any dialog box from the interface, which makes the skins2 interface rather useless. Install the Qt development package or alternatively you can also configure with: --disable-qt --disable-skins2.])
  ], [
    enable_skins2="no"
    AC_MSG_WARN([The skins2 module depends on the Qt interface, Qt is not built so skins2 is disabled.])
  ])
])

dnl
dnl Skins2 module
dnl
AC_ARG_ENABLE([skins2],
  AS_HELP_STRING([--enable-skins2], [skins interface module (default auto)]))
AS_IF([test "${enable_skins2}" != "no"], [
  have_skins_deps="yes"

  dnl Win32
  AS_IF([test "${SYS}" = "mingw32"], [
    VLC_ADD_CPPFLAGS([skins2],[-U_OFF_T_ -U_off_t -DWIN32_SKINS])
    VLC_ADD_LIBS([skins2],[-lgdi32 -lole32 -luuid -lmsimg32])
  dnl OS/2
  ], [test "${SYS}" = "os2"], [
    VLC_ADD_CPPFLAGS([skins2],[ -DOS2_SKINS])
  dnl Linux/Unix
  ], [
    PKG_CHECK_MODULES([XPM], [xpm],, [have_skins_deps="no"])
    PKG_CHECK_MODULES([XINERAMA], [xinerama],, [have_skins_deps="no"])
    PKG_CHECK_MODULES([XCURSOR], [xcursor],, [have_skins_deps="no"])
    PKG_CHECK_MODULES([XEXT], [xext],, [have_skins_deps="no"])
    VLC_ADD_CPPFLAGS([skins2],[${X_CFLAGS} ${XEXT_CFLAGS} ${XPM_CFLAGS} -DX11_SKINS])
    VLC_ADD_LIBS([skins2],[${X_LIBS} ${X_PRE_LIBS} ${XEXT_LIBS} ${XPM_LIBS} ${XINERAMA_LIBS} ${XCURSOR_LIBS} -lX11])
  ])

  dnl we need freetype
  AS_IF([test "${have_freetype}" != "yes"], [
    have_skins_deps="no"
  ])

  AS_IF([test "${have_skins_deps}" = "no"], [
    AS_IF([test "x${enable_skins2}" = "x"], [
      AC_MSG_WARN([Skins2 interface disabled due to missing dependencies.])
    ], [
      AC_MSG_ERROR([Skins2 interface requires FreeType, libxcursor, libxpm, libxext and libxinerama])
    ])
    enable_skins2="no"
  ], [
    ALIASES="${ALIASES} svlc"
    enable_skins2="yes"
  ])
])
AM_CONDITIONAL([BUILD_SKINS], [test "${enable_skins2}" = "yes"])

AC_ARG_ENABLE([libtar],
  AS_HELP_STRING([--enable-libtar], [libtar support for skins2 (default auto)]))
AS_IF([test "${enable_skins2}" = "yes" && test "${enable_libtar}" != "no"], [
  AC_CHECK_HEADERS([libtar.h], [
    VLC_ADD_LIBS([skins2],[-ltar])
  ])
])

dnl
dnl  MacOS X gui module
dnl
AC_ARG_ENABLE([macosx],
  AS_HELP_STRING([--enable-macosx], [Mac OS X gui support (default enabled on Mac OS X)]))
if test "x${enable_macosx}" != "xno" -a "${SYS}" = "darwin"
then

  dnl
  dnl If possible, use xcrun to find the right ibtool
  dnl

  AC_PATH_PROG([XCRUN], [xcrun], [no])
  AS_IF([test ! "x${XCRUN}" = "xno"], [
    AC_MSG_CHECKING([for ibtool (using xcrun)])
    XIB="$(eval $XCRUN -f ibtool 2>/dev/null || echo no)"
    AC_MSG_RESULT([${XIB}])
  ], [
    AC_MSG_WARN([Looking for tools without using xcrun])
  ])

  AS_IF([test "x${XIB}" = "xno"], [
    AC_PATH_PROG(XIB, [ibtool], [no])
    AS_IF([test "x${XIB}" = "xno"], [
      AC_MSG_ERROR([ibtool was not found, but is required for --enable-macosx])
    ])
  ])
fi
AM_CONDITIONAL([ENABLE_MACOSX_UI], [test "$enable_macosx" != "no" -a "${SYS}" = "darwin"])

dnl
dnl  MacOS X sparkle update support
dnl
AC_ARG_ENABLE([sparkle],
  AS_HELP_STRING([--enable-sparkle], [Sparkle update support for OS X (default enabled on Mac OS X)]))
if test "x${enable_sparkle}" != "xno" -a "${HAVE_OSX}" = "1"
then
  if test ! -d ${CONTRIB_DIR}/Sparkle.framework -a ! -d ${CONTRIB_DIR}/Frameworks/Sparkle.framework
  then
    AC_MSG_ERROR([Sparkle framework is required and was not found in ${CONTRIB_DIR}])
  fi

  AC_DEFINE([HAVE_SPARKLE], [1], [Define to 1 if sparkle is enabled.])
fi
AM_CONDITIONAL([HAVE_SPARKLE], [test "$enable_sparkle" != "no"])

dnl
dnl  MacOS X breakpad creash reporter support
dnl
AC_ARG_WITH([breakpad],
  AS_HELP_STRING([--with-breakpad=URL], [Enable Breakpad reporter using given URL]), [], [with_breakpad=no])

AS_IF([test "$with_breakpad" != "no"], [

  AS_IF([test "$with_breakpad" = "yes"], [
    AC_MSG_ERROR([You need to supply the CrashDragon Server URL as value for --with-breakpad])
  ])

  AS_IF([test "$HAVE_OSX" = "1"], [
    AC_MSG_CHECKING([for Breakpad framework])
    AS_IF([test ! -d ${CONTRIB_DIR}/Breakpad.framework -a \
                ! -d ${CONTRIB_DIR}/Frameworks/Breakpad.framework], [
      AC_MSG_RESULT([no])
      with_breakpad="no"
      AC_MSG_ERROR([Breakpad framework is required and was not found in ${CONTRIB_DIR}])
    ], [
      AC_MSG_RESULT([yes])
    ])
  ],[
    PKG_CHECK_MODULES([BREAKPAD], [breakpad-client], [
      LDFLAGS="${LDFLAGS} -Wl,--build-id"
    ], [
      AC_MSG_ERROR(["breakpad not found"])
    ])
  ])
])

AM_CONDITIONAL([HAVE_BREAKPAD], [test "$with_breakpad" != "no"])
AS_IF([test "$with_breakpad" != "no"], [
  AC_SUBST([BREAKPAD_URL], ["${with_breakpad}"])
])

dnl
dnl  Minimal Mac OS X module
dnl
AC_ARG_ENABLE([minimal-macosx],
  AS_HELP_STRING([--enable-minimal-macosx], [Minimal Mac OS X support (default disabled)]))
if test "${enable_minimal_macosx}" = "yes" -a "${SYS}" = "darwin"
then
  VLC_ADD_PLUGIN([minimal_macosx])
fi

dnl
dnl Mac OS X fancy dmg build tool
dnl
if test "${HAVE_OSX}" = "1"
then
  AC_PATH_PROG(DMGBUILD, dmgbuild, no)
  AS_IF([test "x${DMGBUILD}" = "xno"], [
    AC_MSG_WARN([dmgbuild not found -- unable to build fancy DMGs])
  ])
fi
AM_CONDITIONAL([HAVE_DMGBUILD], [test "x$DMGBUILD" != "xno" -a "${HAVE_OSX}" = "1"])

dnl
dnl  VideoToolbox plugins
AC_CHECK_HEADERS([VideoToolbox/VideoToolbox.h], [
    VLC_ADD_PLUGIN([videotoolbox])
    VLC_ADD_PLUGIN([cvpx])
  ])

dnl
dnl  AudioToolbox MIDI plugin
AC_CHECK_HEADERS([AudioToolbox/AudioToolbox.h], [
  VLC_ADD_PLUGIN([audiotoolboxmidi])
])

dnl
dnl  ncurses module
dnl
AC_ARG_ENABLE([ncurses],
AS_HELP_STRING([--disable-ncurses],[ncurses text-based interface (default auto)]))
have_ncurses="no"
AS_IF([test "${enable_ncurses}" != "no"] ,[
  PKG_CHECK_MODULES([NCURSES], [ncursesw], [
    have_ncurses="yes"
    ALIASES="${ALIASES} nvlc"
  ], [
    AS_IF([test -n "${enable_ncurses}"], [
      AC_MSG_ERROR([${NCURSES_PKG_ERRORS}.])
    ])
  ])
])
AM_CONDITIONAL([HAVE_NCURSES], [test "${have_ncurses}" = "yes"])

dnl
dnl  Lirc plugin
dnl
AC_ARG_ENABLE([lirc],
  AS_HELP_STRING([--enable-lirc], [lirc support (default disabled)]))
have_lirc="no"
AS_IF([test "${enable_lirc}" = "yes"], [
  AC_CHECK_HEADER(lirc/lirc_client.h, [
    AC_CHECK_LIB([lirc_client], [lirc_init], [
      have_lirc="yes"
    ])
  ])
])
AM_CONDITIONAL([HAVE_LIRC], [test "${have_lirc}" = "yes"])

dnl
dnl  Bitstream
dnl
PKG_CHECK_MODULES([BITSTREAM], [bitstream > 1.4],[have_bitstream="yes"], [have_bitstream="no"])
AM_CONDITIONAL([HAVE_BITSTREAM], [test "${have_bitstream}" = "yes"])

dnl
dnl  SRT plugin
dnl
PKG_ENABLE_MODULES_VLC([SRT], [access_srt access_output_srt], [srt >= 1.3.0], [SRT input/output plugin], [auto], [], [], [-DENABLE_SRT])

EXTEND_HELP_STRING([Visualisations and Video filter plugins:])
dnl
dnl  goom visualization plugin
dnl
PKG_ENABLE_MODULES_VLC([GOOM], [], [libgoom2], [goom visualization plugin], [auto])

dnl
dnl libprojectM visualization plugin
dnl
AC_ARG_ENABLE([projectm],
  AS_HELP_STRING([--enable-projectm], [projectM visualization plugin (default enabled)]))
AS_IF([test "${enable_projectm}" != "no"],
  [
    PKG_CHECK_MODULES(PROJECTM, libprojectM,
    [
      VLC_ADD_PLUGIN([projectm])
      PKG_CHECK_EXISTS([libprojectM >= 2.0.0],
        [ AC_DEFINE([HAVE_PROJECTM2], 1, [Define to 1 if using libprojectM 2.x]) ],
        [ AC_MSG_WARN( [Using libprojectM version 1] )
      ])
    ],[
      AC_MSG_WARN([${PROJECTM_PKG_ERRORS}.])
    ])
  ])

dnl
dnl Vovoid VSXu visualization plugin
dnl
AC_ARG_ENABLE([vsxu],
  AS_HELP_STRING([--enable-vsxu], [Vovoid VSXu visualization plugin (default auto)]))
AS_IF([test "${enable_vsxu}" != "no"],
  [
    PKG_CHECK_MODULES(VSXU, libvsxu,
    [
      VLC_ADD_PLUGIN([vsxu])
    ],[
      AC_MSG_WARN([${VSXU_PKG_ERRORS}.])
    ])
  ])

EXTEND_HELP_STRING([Service Discovery plugins:])
dnl
dnl  Avahi-Zeroconf services discovery
PKG_ENABLE_MODULES_VLC([AVAHI], [], [avahi-client >= 0.6], [Zeroconf services discovery], [auto])

dnl
dnl  libudev services discovery
PKG_ENABLE_MODULES_VLC([UDEV], [], [libudev >= 142], [Linux udev services discovery], [auto])

dnl
dnl MTP devices services discovery
PKG_ENABLE_MODULES_VLC([MTP], [mtp access_mtp], [libmtp >= 1.0.0], [MTP devices support], [auto])

dnl
dnl UPnP Plugin (Intel SDK)
dnl
PKG_ENABLE_MODULES_VLC([UPNP], [upnp], [libupnp], [Intel UPNP SDK],[auto])

dnl
dnl mDNS using libmicrodns
dnl
PKG_ENABLE_MODULES_VLC([MICRODNS], [], [microdns >= 0.1.2], [mDNS services discovery], [auto])


EXTEND_HELP_STRING([Misc options:])

dnl
dnl  libxml2 module
dnl
PKG_ENABLE_MODULES_VLC([LIBXML2], [xml], [libxml-2.0 >= 2.5], [libxml2 support],[auto])


dnl
dnl libgcrypt
dnl
AC_ARG_ENABLE([libgcrypt],
  AS_HELP_STRING([--disable-libgcrypt], [gcrypt support (default enabled)]))
AC_ARG_VAR([GCRYPT_CFLAGS], [C compiler flags for gcrypt])
AC_ARG_VAR([GCRYPT_LIBS], [linker flags flags for gcrypt])

# require libgcrypt >= 1.6.0
AS_IF([test "${enable_libgcrypt}" != "no"], [
  AC_CACHE_CHECK([for gcrypt 1.6.0 or later], [ac_cv_lib_gcrypt], [
    VLC_SAVE_FLAGS
    CFLAGS="${CFLAGS} ${GCRYPT_CFLAGS}"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
[#include <gcrypt.h>
#if GCRYPT_VERSION_NUMBER < 0x010600
#error
#endif]])], [
      ac_cv_lib_gcrypt=yes
    ], [
      ac_cv_lib_gcrypt=no
    ])
    VLC_RESTORE_FLAGS
  ])

  AS_IF([test "${ac_cv_lib_gcrypt}" = "yes"], [
    AC_DEFINE([HAVE_GCRYPT], 1, [Defined if having gcrypt])
  ], [
    AS_IF([test "${enable_libgcrypt}" = "yes"], [
      AC_MSG_ERROR([libgcrypt version 1.6.0 or higher not found. Install libgcrypt or pass --disable-libgcrypt.])
    ])
  ])
])

AS_IF([test -z "$GCRYPT_LIBS"], [
  GCRYPT_LIBS="-lgcrypt -lgpg-error"
])

AC_SUBST([GCRYPT_CFLAGS])
AC_SUBST([GCRYPT_LIBS])
AM_CONDITIONAL([HAVE_GCRYPT], [test "${ac_cv_lib_gcrypt}" = "yes"])

dnl
dnl TLS/SSL
dnl
AC_ARG_ENABLE([gnutls],
  AS_HELP_STRING([--enable-gnutls], [GNU TLS TLS/SSL support (default enabled)]))
AS_IF([test "${enable_gnutls}" != "no"], [
  PKG_CHECK_MODULES(GNUTLS, [gnutls >= 3.5.0], [
    VLC_ADD_PLUGIN([gnutls])
  ], [
    AS_IF([test -n "${enable_gnutls}"], [
      AC_MSG_ERROR([${GNUTLS_PKG_ERRORS}.)])
    ])
  ])
])


dnl
dnl Taglib plugin
dnl
AC_ARG_ENABLE([taglib],
  AS_HELP_STRING([--disable-taglib],
    [do not use TagLib (default enabled)]))
AS_IF([test "${enable_taglib}" != "no"], [
  PKG_CHECK_MODULES(TAGLIB, taglib >= 1.11, [
    VLC_ADD_PLUGIN([taglib])
  ], [
    AC_MSG_WARN([${TAGLIB_PKG_ERRORS}.])])
])

dnl
dnl  libsecret
dnl
PKG_ENABLE_MODULES_VLC([SECRET], [], [libsecret-1 >= 0.18], [use libsecret for keystore], [auto])

dnl
dnl  kwallet
dnl
AC_ARG_ENABLE([kwallet],
  AS_HELP_STRING([--enable-kwallet],
    [use kwallet (via D-Bus) for keystore (default enabled)]))
AS_IF([test "${enable_kwallet}" != "no" -a "${have_dbus}" = "yes"], [
  VLC_ADD_PLUGIN([kwallet])
])

dnl
dnl update checking system
dnl
AC_ARG_ENABLE([update-check],
  AS_HELP_STRING([--enable-update-check], [update checking system (default disabled)]))
if test "${enable_update_check}" = "yes"
then
 AS_IF([test "${ac_cv_lib_gcrypt}" != "yes"],[
    AC_MSG_ERROR([libgcrypt is required for update checking system]) ])
fi
AM_CONDITIONAL([UPDATE_CHECK], [test "${enable_update_check}" = "yes"])

dnl
dnl OS X notification plugin
dnl
AC_ARG_ENABLE([osx_notifications],
  AS_HELP_STRING([--enable-osx-notifications],
    [macOS notification plugin (default disabled)]),,
  [enable_osx_notifications=no])
AS_IF([test "${enable_osx_notifications}" != "no"], [
  VLC_ADD_PLUGIN([osx_notifications])
])

dnl
dnl Libnotify notification plugin
dnl
PKG_ENABLE_MODULES_VLC([NOTIFY], [], [libnotify], [libnotify notification], [auto])

PKG_ENABLE_MODULES_VLC([MEDIALIBRARY], [medialibrary], [medialibrary >= 0.8.0], (medialibrary support), [auto])

dnl
dnl  Endianness check
dnl
AC_C_BIGENDIAN
AS_IF([test "${ac_cv_c_bigendian}" = "yes"], [
  DEFS_BIGENDIAN="-DWORDS_BIGENDIAN=1"
], [
  DEFS_BIGENDIAN=""
])
AC_SUBST([DEFS_BIGENDIAN])

dnl
dnl Where to install KDE solid .desktop
dnl
AC_ARG_WITH(kde-solid,
  AS_HELP_STRING([--with-kde-solid=PATH],
                 [KDE Solid actions directory (auto)]),, [

if test "${SYS}" != "mingw32" -a "${SYS}" != "darwin" -a "${HAVE_ANDROID}" != "1" ; then
  with_kde_solid="yes"
fi
])
soliddatadir=""
AS_IF([test "${with_kde_solid}" != "no"], [
  AS_IF([test "${with_kde_solid}" = "yes"], [
    kde4datadir="`${KDE4_CONFIG} --install data`"
    AS_IF([test "x$kde4datadir" = "x"], [kde4datadir='${datadir}/kde4/apps'])
    soliddatadir="${kde4datadir}/solid/actions"
  ], [
    soliddatadir="${with_kde_solid}"
  ])
])
AC_SUBST([soliddatadir])
AM_CONDITIONAL([KDE_SOLID], [test "x${soliddatadir}" != "x"])

dnl
dnl  Find tools for win32 packaging
dnl
AS_IF([test "${SYS}" = "mingw32"], [
    dnl
    dnl Find makensis required to build the NSIS installer
    dnl
    nsis_major_required="3"
    nsis_version_ok="no"
    AC_PATH_PROG([MAKENSIS], [makensis], [no])
    AS_IF([test "$MAKENSIS" != "no"], [
        # Check version
        AC_MSG_CHECKING([makensis version])
        nsis_version=$($MAKENSIS -VERSION)
        AC_MSG_RESULT([$nsis_version])
        nsis_version_major=$(echo "$nsis_version" | awk -F \. '{print $1}' | sed 's/v//')
        # Check if major version is a valid number
        AS_IF([test -n "$nsis_version_major" && \
               test "$nsis_version_major" -eq "$nsis_version_major" 2> /dev/null], [
            # Do the version check using major version
            AS_IF([test "$nsis_version_major" -lt "$nsis_major_required"], [
                AC_MSG_WARN([makensis too old, (needs ${nsis_major_required}.x, have ${nsis_version_major}.x)])
            ], [
                nsis_version_ok="yes"
            ])
        ], [
            AC_MSG_WARN([failed to check the makensis version])
        ])
    ])
    dnl
    dnl Find 7zip
    dnl
    AC_PATH_PROG([SEVENZIP], [7z], [7z])
])
AM_CONDITIONAL([HAVE_MAKENSIS], [test "x$MAKENSIS" != "xno" && test "$nsis_version_ok" = "yes"])

dnl
dnl Check wether we have the PROCESS_MITIGATION_IMAGE_LOAD_POLICY
dnl It should only be available when building for win10, but some SDKs define it unconditionnaly
dnl
if test "${SYS}" = "mingw32"; then
    AC_CHECK_TYPES([PROCESS_MITIGATION_IMAGE_LOAD_POLICY],,,[#include <windows.h>])
fi

EXTEND_HELP_STRING([Components:])

dnl
dnl  the VLC binary
dnl
AC_ARG_ENABLE([vlc],
  AS_HELP_STRING([--enable-vlc], [build the VLC media player (default enabled)]))
AM_CONDITIONAL([BUILD_VLC], [test "${enable_vlc}" != "no"])

dnl
dnl Fuzzer (at the end not to mess dependencies FLAGS)
dnl
AS_VAR_IF(with_libfuzzer, no, [], [
    LIBS="-lstdc++ ${LIBM}"
    AS_IF([test "${with_libfuzzer}" != "yes"], [
      LIBS="$LIBS -L${with_libfuzzer}"
    ])
    LIBS="$LIBS -lFuzzer"
    AX_CHECK_COMPILE_FLAG(-fsanitize-coverage=trace-pc-guard, [
        AX_APPEND_FLAG([-fsanitize-coverage=trace-pc-guard])
        AX_APPEND_FLAG([-fsanitize-coverage=trace-pc-guard], [CXXFLAGS])
        AX_APPEND_COMPILE_FLAGS([-fsanitize-coverage=trace-cmp])
        AX_APPEND_COMPILE_FLAGS([-fsanitize-coverage=trace-cmp], [CXXFLAGS])
    ], [
        AX_CHECK_COMPILE_FLAG(-fsanitize-coverage=trace-pc, [
            AX_APPEND_FLAG([-fsanitize-coverage=trace-pc])
            AX_APPEND_FLAG([-fsanitize-coverage=trace-pc], [CXXFLAGS])
        ], [
            AC_MSG_ERROR([Sanitizer coverage not supported by your compiler])
        ])
    ])
])

dnl
dnl  Stuff used by the program
dnl
VERSION_MESSAGE="${VERSION} ${CODENAME}"
COPYRIGHT_MESSAGE="Copyright © ${COPYRIGHT_YEARS} the VideoLAN team"
AC_DEFINE_UNQUOTED([VERSION_MESSAGE], "${VERSION_MESSAGE}", [Simple version string])
AC_DEFINE_UNQUOTED([COPYRIGHT_MESSAGE], "${COPYRIGHT_MESSAGE}", [Copyright string])
AC_DEFINE_UNQUOTED([COPYRIGHT_YEARS], "${COPYRIGHT_YEARS}", [The copyright years])
AC_DEFINE_UNQUOTED([CONFIGURE_LINE], "${CONFIGURE_LINE}", [The ./configure command line])
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_MAJOR],${VERSION_MAJOR}, [version major number])
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_MINOR],${VERSION_MINOR}, [version minor number])
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_REVISION],${VERSION_REVISION}, [version revision number])
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_EXTRA],${VERSION_EXTRA}, [version extra number])
AC_DEFINE_UNQUOTED([PACKAGE_VERSION_DEV],"${VERSION_DEV}", [version development string])
AC_SUBST([COPYRIGHT_MESSAGE])
AC_SUBST([VERSION_MESSAGE])
AC_SUBST([VERSION_MAJOR])
AC_SUBST([VERSION_MINOR])
AC_SUBST([VERSION_REVISION])
AC_SUBST([VERSION_EXTRA])
AC_SUBST([COPYRIGHT_YEARS])
AC_DEFINE_UNQUOTED([VLC_COMPILE_BY], "`whoami|sed -e 's/\\\/\\\\\\\/g'`", [user who ran configure])
AC_DEFINE_UNQUOTED([VLC_COMPILE_HOST], "`hostname -f 2>/dev/null || hostname`", [host which ran configure])
AC_DEFINE_UNQUOTED([VLC_COMPILER], "`$CC -v 2>&1 | tail -n 1 | sed -e 's/ *$//'`", [compiler])
dnl
dnl  Handle substvars that use $(top_srcdir)
dnl
CPPFLAGS="-I\$(top_srcdir)/include -I\$(top_builddir)/include ${CPPFLAGS}"

dnl
dnl  Configuration is finished
dnl
AC_SUBST([SYS])
AC_SUBST([ARCH])
AC_SUBST([ALIASES])
AC_SUBST([ASM])
AC_SUBST([MOC])
AC_SUBST([RCC])
AC_SUBST([UIC])
AC_SUBST([WINDRES])
AC_SUBST([WINE_SDK_PATH])
AC_SUBST([LIBEXT])
AC_SUBST([AM_CPPFLAGS])
AC_SUBST([MACOSX_DEPLOYMENT_TARGET])
AC_SUBST([FILE_LIBVLCCORE_DLL])
AC_SUBST([FILE_LIBVLC_DLL])

AC_CONFIG_FILES([
  Makefile
  doc/Makefile
  modules/Makefile
  m4/Makefile
  po/Makefile.in
  share/Makefile
  compat/Makefile
  src/Makefile
  lib/Makefile
  bin/Makefile
  test/Makefile
  modules/hw/mmal/Makefile
  share/vlc.appdata.xml.in
])

AM_COND_IF([HAVE_WIN32], [
  AC_CONFIG_FILES([
    extras/package/win32/NSIS/spad.nsi
    extras/package/win32/NSIS/vlc.win32.nsi
    extras/package/win32/msi/config.wxi
  ])
])

AM_COND_IF([HAVE_DARWIN], [
  AC_CONFIG_FILES([
    modules/gui/macosx/UI/InfoPlist.strings:modules/gui/macosx/Resources/InfoPlist.strings.in
    share/macosx/Info.plist:share/Info.plist.in
  ])
])

dnl Generate makefiles
AC_OUTPUT

dnl Do we have to use make or gmake ?
if make --version 2>&1|grep -q GNU
then
    # GNU make, all seems good
    MAKE=make
else
    # Known GNU make alias (on BSD)
    MAKE=gmake
fi
dnl Shortcut to nice compile message
if test -n $SHELL; then
  SHELL=${CONFIG_SHELL-/bin/sh}
fi
rm -f compile
compile_top_srcdir=`sed -ne "s/^top_srcdir *= *//p" < Makefile`
echo "#! $SHELL
rm -f .error\$\$
ERROR=0
export PATH=\"$PATH\" LANG=C
($MAKE V=1 \$@ 2>&1 || touch .error\$\$)| $compile_top_srcdir/extras/buildsystem/make.pl
test -f .error\$\$ && ERROR=1
rm -f .error\$\$
exit \$ERROR" >compile
chmod a+x compile

echo "
libvlc configuration
--------------------
version               : ${VERSION}
system                : ${SYS}
architecture          : ${ARCH}
optimizations         : ${enable_optimizations}"
if test "${enable_vlc}" != "no"; then
echo "vlc aliases           :${ALIASES}"
else
echo "build vlc executable  : no"
fi
echo "
To build vlc and its plugins, type \`$MAKE', or \`./compile' if you like nice colors."
