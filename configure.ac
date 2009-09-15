dnl Autoconf settings for vlc

AC_COPYRIGHT([Copyright 2002-2009 the VideoLAN team])

AC_INIT(vlc, 1.1.0-git)
VERSION_MAJOR="1"
VERSION_MINOR="1"
VERSION_REVISION="0"
VERSION_EXTRA="-git"
PKGDIR="vlc"
AC_SUBST(PKGDIR)

CONFIGURE_LINE="`echo "$0 $ac_configure_args" | sed -e 's/\\\/\\\\\\\/g'`"
CODENAME="Yellow Bastard"
COPYRIGHT_YEARS="1996-2009"

AC_PREREQ(2.59c)
AC_CONFIG_SRCDIR(src/libvlc.c)
AC_CONFIG_AUX_DIR(autotools)
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_LIBOBJ_DIR(compat)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE(tar-ustar)
AM_CONFIG_HEADER(config.h)

# Disable with "./configure --disable-silent-rules" or "make V=1"
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])


dnl Too many people are not aware of maintainer mode:
dnl If you want to use it, you definitely know what you are doing, so
dnl you can specify "--disable-maintainer-mode". But if you want the default
dnl automake behavior, you've likely never heard of maintainer mode, so we
dnl can't expect you to enable it manually.

AS_IF([test "x${enable_maintainer_mode}" != "xno"],
    [enable_maintainer_mode="yes"])
AM_MAINTAINER_MODE

dnl
dnl Directories
dnl
dnl vlcincludedir="\${includedir}/\${PKGDIR}"
dnl AC_SUBST(vlcincludedir)
vlcdatadir="\${datadir}/\${PKGDIR}"
AC_SUBST(vlcdatadir)
vlclibdir="\${libdir}/\${PKGDIR}"
AC_SUBST(vlclibdir)

dnl
dnl  Deprecated options
dnl  to notify packagers that stuff has changed
dnl
AC_ARG_ENABLE(python-bindings,
  [  --enable-python-bindings Always fails for historical reasons)],,
  [enable_python_bindings="no"])
AS_IF([test "${enable_python_bindings}" != "no"], [
  AC_MSG_ERROR([Python bindings are now built from a separate source package])
])

AC_ARG_ENABLE(java-bindings,
  [  --enable-java-bindings  Always fails for historical reasons],,
  [enable_java_bindings="no"])
AS_IF([test "${enable_java_bindings}" != "no"], [
  AC_MSG_ERROR([Java bindings are now built from a separate source package])
])

dnl
dnl  Save *FLAGS
dnl
VLC_SAVE_FLAGS

dnl
dnl Check for tools
dnl
AC_PROG_CC_C99
AC_USE_SYSTEM_EXTENSIONS
AC_DEFINE([_FORTIFY_SOURCE], 2, [Define to '2' to get glibc warnings.])

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

dnl Allow binary package maintainer to pass a custom string to avoid
dnl cache problem
AC_ARG_WITH(binary-version,
	AS_HELP_STRING([--with-binary-version=STRING],
	     [To avoid plugins cache problem between binary version]),[],[])
AS_IF([test -n "${with_binary_version}"],[
      AC_DEFINE_UNQUOTED([DISTRO_VERSION],["${with_binary_version}"],
			 [Binary specific version])
      ])


dnl
dnl  Check for the contrib directory
dnl
AC_ARG_WITH(contrib,
    [  --without-contrib       do not use the libraries in CONTRIB_DIR],[],[])
  AS_IF([test "${with_contrib}" != "no"],[
    AC_ARG_VAR([CONTRIB_DIR], [directory containing pre-built contrib, overriding extras/contrib])
    if test -z "$CONTRIB_DIR"
    then
      topdir="`dirname $0`"
      if test "`echo \"$topdir\" | cut -c 1`" != "/"; then
         topdir="`pwd`/$topdir"
      fi
      CONTRIB_DIR=${topdir}/extras/contrib
    fi
    AC_MSG_CHECKING([for libs in ${CONTRIB_DIR}])
  AS_IF([test -d ${CONTRIB_DIR}/lib],[
   AS_IF([test "`grep HOST ${CONTRIB_DIR}/config.mak 2>/dev/null|awk '{print $3}'`" != "`$CC -dumpmachine`"],[
      if test "${with_contrib}" = "yes"; then
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([ not using the libs in extras/contrib as it is not the same host])
      else
        AC_MSG_RESULT([no])
        AC_MSG_WARN([ not using the libs in extras/contrib as it is not the same host])
      fi
     ],[
    AC_MSG_RESULT([yes])
    export PATH=${CONTRIB_DIR}/bin:$PATH
    CPPFLAGS="${CPPFLAGS} -I${CONTRIB_DIR}/include"
    CPPFLAGS_save="${CPPFLAGS_save} -I${CONTRIB_DIR}/include"
    CFLAGS="${CFLAGS} -I${CONTRIB_DIR}/include"
    CFLAGS_save="${CFLAGS_save} -I${CONTRIB_DIR}/include"
    CXXFLAGS="${CXXFLAGS} -I${CONTRIB_DIR}/include"
    CXXFLAGS_save="${CXXFLAGS_save} -I${CONTRIB_DIR}/include"
    OBJCFLAGS="${OBJCFLAGS} -I${CONTRIB_DIR}/include"
    OBJCFLAGS_save="${OBJCFLAGS_save} -I${CONTRIB_DIR}/include"
    if test $build = $host -o "$PKG_CONFIG_LIBDIR"; then
        export PKG_CONFIG_PATH=${CONTRIB_DIR}/lib/pkgconfig:$PKG_CONFIG_PATH
    else
        export PKG_CONFIG_LIBDIR=${CONTRIB_DIR}/lib/pkgconfig
    fi
    LDFLAGS="${LDFLAGS} -L${CONTRIB_DIR}/lib"
    LDFLAGS_save="${LDFLAGS_save} -L${CONTRIB_DIR}/lib"

    if test -z $with_mozilla_sdk_path; then
       with_mozilla_sdk_path=${CONTRIB_DIR}/gecko-sdk
    fi
    if test -z $with_cyberlink_tree; then
       with_cyberlink_tree=${CONTRIB_DIR}/src/clinkcc
    fi

    if test ".`uname -s`" = ".Darwin"; then
      export LD_LIBRARY_PATH=${CONTRIB_DIR}/lib:$LD_LIBRARY_PATH
      export DYLD_LIBRARY_PATH=${CONTRIB_DIR}/lib:$DYLD_LIBRARY_PATH
    elif test ".`uname -s`" = ".BeOS"; then
      export LIBRARY_PATH=${CONTRIB_DIR}/lib:$LIBRARY_PATH
      export BELIBRARIES=${CONTRIB_DIR}/lib:$BELIBRARIES
    fi
    ])
  ],[
    AC_MSG_RESULT([no])
  ])
])
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
      openbsd*)
        CPPFLAGS_save="${CPPFLAGS_save} -I/usr/local/include -I/usr/X11R6/include"
        CPPFLAGS="${CPPFLAGS_save}"
        LDFLAGS_save="${LDFLAGS_save} -L/usr/local/lib -L/usr/X11R6/lib/"
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
    VLC_ADD_LDFLAGS([libvlc],[-Wl,-undefined,dynamic_lookup])
    VLC_ADD_LDFLAGS([avcodec avformat swscale postproc i420_rgb_mmx x264],[-Wl,-read_only_relocs,suppress])
    VLC_ADD_CFLAGS([motion],[-fconstant-cfstrings])
    VLC_ADD_LDFLAGS([libvlccore],[-Wl,-framework,CoreFoundation])
    VLC_ADD_LDFLAGS([motion],[-Wl,-framework,IOKit,-framework,CoreFoundation])
    AC_ARG_ENABLE(macosx-defaults,
      AS_HELP_STRING([--enable-macosx-defaults],[Build the default configuration on Mac OS X (default enabled)]))
    if test "x${enable_macosx_defaults}" != "xno"
    then
        echo ""
        echo "Building with Mac OS X defaults:"
        with_macosx_version_min="10.5"
        echo "  Assuming --with-macosx-version-min=10.5"
        with_macosx_sdk="/Developer/SDKs/MacOSX${with_macosx_version_min}.sdk"
        echo "  Assuming --with-macosx-sdk=/Developer/SDKs/MacOSX${with_macosx_version_min}.sdk"
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
        enable_ncurses="yes"
        echo "  Assuming --enable-ncurses"
        enable_twolame="yes"
        echo "  Assuming --enable-twolame"
        enable_realrtsp="yes"
        echo "  Assuming --enable-realrtsp"
        enable_libass="yes"
        echo "  Assuming --enable-libass"
        enable_asademux="yes"
        echo "  Assuming --enable-asademux"

        enable_skins2="no"
        echo "  Assuming --disable-skins2"
        enable_x11="no"
        echo "  Assuming --disable-x11"
        enable_xcb="no"
        echo "  Assuming --disable-xcb"
        enable_glx="no"
        echo "  Assuming --disable-glx"
        enable_xvideo="no"
        echo "  Assuming --disable-xvideo"
    fi
    dnl
    dnl  Check for Mac OS X SDK settings
    dnl
    AC_ARG_WITH(macosx-sdk,
       [  --with-macosx-sdk=DIR   compile using the SDK in DIR])
    if test "${with_macosx_sdk}" != "" ; then
        CPP="${CPP} -isysroot ${with_macosx_sdk}"
        CC="${CC} -isysroot ${with_macosx_sdk}"
        CXX="${CXX} -isysroot ${with_macosx_sdk}"
        OBJC="${OBJC} -isysroot ${with_macosx_sdk}"
        LD="${LD} -syslibroot ${with_macosx_sdk}"
    fi
    AC_ARG_WITH(macosx-version-min,
       [  --with-macosx-version-min=VERSION  compile for MacOSX VERSION and above])
    if test "${with_macosx_version_min}" != "" ; then
        CPP="${CPP} -mmacosx-version-min=${with_macosx_version_min}"
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
  darwin9*)
    if (test ".`uname -p`" = ".i386"); then
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
    ;;
  *mingw32* | *cygwin* | *wince* | *mingwce*)
    AC_CHECK_TOOL(WINDRES, windres, :)
    AC_CHECK_TOOL(OBJCOPY, objcopy, :)

    case "${host_os}" in
      *wince* | *mingwce* | *mingw32ce*)
        SYS=mingwce
        dnl Sadly CeGCC still needs non-wince macros
        AC_DEFINE([_WIN32_WINNT], 0x0501, [Define to '0x0500' for Windows 2000 APIs.])
        ;;
      *mingw32*)
        SYS=mingw32
        AC_DEFINE([_WIN32_WINNT], 0x0500, [Define to '0x0500' for Windows 2000 APIs.])
        ;;
      *cygwin*)
        dnl Check if we are using the mno-cygwin mode in which case we are
        dnl actually dealing with a mingw32 compiler.
        AC_DEFINE([_WIN32_WINNT], 0x0500, [Define to '0x0500' for Windows 2000 APIs.])
        AC_EGREP_CPP(yes,
            [#ifdef WIN32
             yes
             #endif],
            SYS=mingw32, AC_MSG_ERROR([VLC requires -mno-cygwin]))
        ;;
    esac

    if test "${SYS}" = "mingw32"; then
        # add ws2_32 for closesocket, select, recv
        VLC_ADD_LIBS([libvlccore],[-lws2_32 -lnetapi32 -lwinmm])
        VLC_ADD_LDFLAGS([vlc],[-mwindows])
        VLC_ADD_LIBS([activex mozilla],[-lgdi32])
        VLC_ADD_LIBS([cdda vcdx cddax sdl_image aout_sdl vout_sdl],[-lwinmm])
        VLC_ADD_LIBS([access_http access_mms access_udp access_tcp access_ftp access_rtmp access_output_udp access_output_shout access_output_rtmp sap slp http stream_out_standard stream_out_rtp stream_out_raop vod_rtsp access_realrtsp rtp telnet rc netsync gnutls growl_udp flac ts audioscrobbler lua remoteosd zvbi audiobargraph_a],[-lws2_32])
        VLC_ADD_LIBS([access_file], [-lshlwapi])
        dnl
        dnl DEP and ASLR options
        dnl
        AC_ARG_ENABLE(peflags,
          [  --enable-peflags          peflags use (default enabled)])
        if test "${enable_peflags}" != "no"
        then
          AC_CHECK_TOOL(PEFLAGS, peflags, :)
        fi
    fi
    if test "${SYS}" = "mingwce"; then
        # add ws2 for closesocket, select, recv
        VLC_ADD_LIBS([libvlccore access_http access_mms access_udp access_tcp access_ftp access_rtmp access_output_udp access_output_rtmp sap http netsync audioscrobbler growl rtp stream_out_rtp remoteosd ts telnet audiobargraph_a],[-lws2])
        VLC_ADD_LIBS([libvlccore],[-lmmtimer])
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
  beos)
    SYS=beos
    CFLAGS_save="${CFLAGS_save} -Wno-multichar"; CFLAGS="${CFLAGS_save}"
    CXXFLAGS_save="${CXXFLAGS_save} -Wno-multichar"; CXXFLAGS="${CXXFLAGS_save}"
    VLC_ADD_CXXFLAGS([beos],[])
    VLC_ADD_LIBS([vlc libvlccore logger],[-lbe])
    VLC_ADD_LIBS([beos],[-lbe -lmedia -ltranslation -ltracker -lgame])
    VLC_ADD_LIBS([dvdnav dvdread],[-ldl])
    VLC_ADD_LIBS([access_file],[-lpoll])
    LDFLAGS_save="${LDFLAGS_save} -lintl"; LDFLAGS="${LDFLAGS_save}"
    dnl Check for BONE
    if test -f /boot/beos/system/lib/libbind.so; then
        VLC_ADD_LIBS([access_file access_ftp access_mms access_output_udp telnet netsync sap libvlccore growl_udp],[-lbind -lsocket])
    else
        VLC_ADD_LIBS([access_file access_ftp access_mms access_output_udp telnet netsync sap libvlccore growl_udp],[-lnet])
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
AM_CONDITIONAL(HAVE_LINUX, [test "${SYS}" = "linux"])
AM_CONDITIONAL(HAVE_WIN32, test "${SYS}" = "mingw32")
AM_CONDITIONAL(HAVE_WINCE, test "${SYS}" = "mingwce")
AM_CONDITIONAL(USE_PEFLAGS, [test "${enable_peflags}" = "yes"])

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
dnl AC_DISABLE_FAST_INSTALL
AC_LIBTOOL_DLOPEN
AC_LIBTOOL_WIN32_DLL
m4_undefine([AC_PROG_F77])
m4_defun([AC_PROG_F77],[])
AC_PROG_LIBTOOL

m4_undefine([AC_DEPLIBS_CHECK_METHOD])
m4_defun([AC_DEPLIBS_CHECK_METHOD],[])

lt_cv_deplibs_check_method=pass_all

AS_IF([test "${enable_shared}" = "no"], [
  AC_MSG_ERROR([VLC is based on plugins. Shared libraries cannot be disabled.])
])

dnl
dnl Gettext stuff
dnl
AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])
VLC_ADD_LIBS([libvlccore vlc], [${LTLIBINTL}])

dnl
dnl Iconv stuff
dnl
AM_ICONV
VLC_ADD_CFLAGS([libvlccore],[${INCICONV}])
VLC_ADD_LIBS([libvlccore],[${LTLIBICONV}])

dnl Check for broken versions of mingw-runtime compatability library
AS_IF([test "${SYS}" = "mingw32" -o "${SYS}" = "mingwce"], [
    AC_MSG_CHECKING(for broken mingw-runtime)
    AC_PREPROC_IFELSE([
#include <_mingw.h>
#if (__MINGW32_MAJOR_VERSION == 3) && (__MINGW32_MINOR_VERSION < 15)
# error Attempting to use mingw-runtime with broken vsnprintf support
#endif
], [
        AC_MSG_RESULT([ok])
], [
        AC_MSG_RESULT([present])
        AC_MSG_ERROR([LibVLC requires mingw-runtime version 3.15 or higher!])
])
    dnl force use of mingw provided c99 *printf over msvcrt
    CPPFLAGS="${CPPFLAGS} -D__USE_MINGW_ANSI_STDIO=1"
    CPPFLAGS_save="${CPPFLAGS_save} -D__USE_MINGW_ANSI_STDIO=1"
])

dnl Check for the need to include the mingwex lib for mingw32
if test "${SYS}" = "mingw32"
then
    AC_CHECK_LIB(mingwex,opendir,
        AC_CHECK_LIB(mingw32,opendir,,
            [VLC_ADD_LIBS([libvlccore],[-lmingwex])])
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

dnl
dnl Buggy glibc prevention. Purposedly not cached.
dnl See sourceware.org bugs 5058 and 5443.
dnl Ubuntu alone has 20 bug numbers for this...
dnl
AC_MSG_CHECKING(for buggy GNU/libc versions)
AC_PREPROC_IFELSE([
#include <limits.h>
#if defined (__GLIBC__) && (__GLIBC__ == 2) \
  && (__GLIBC_MINOR__ >= 5) && (__GLIBC_MINOR__ <= 7)
# error GNU/libc with dcgettext killer bug!
#endif
], [
  AC_MSG_RESULT([not present])
], [
  AC_MSG_RESULT([found])
  AS_IF([test "x${enable_nls}" != "xno" || test "x${enable_mozilla}" != "xno"], [
    AC_MSG_ERROR([Buggy GNU/libc (version 2.5 - 2.7) present. VLC would crash; there is no viable
work-around for this. Check with your distribution vendor on how to update the
glibc run-time. Alternatively, build with --disable-nls --disable-mozilla and
be sure to not use LibVLC from other applications/wrappers.])
  ], [
    AC_DEFINE(DISABLE_BUGGY_GLIBC_CHECK, 1, [Disables runtime check for buggy glibc.])
  ])
])

dnl Plugin compilation stuff

VLC_LIBRARY_SUFFIX

dnl The -DSYS_FOO flag
CPPFLAGS_save="${CPPFLAGS_save} -DSYS_`echo ${SYS} | sed -e 's/-.*//' | tr 'abcdefghijklmnopqrstuvwxyz.' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ_'`"; CPPFLAGS="${CPPFLAGS_save}"

dnl Check for system libs needed
need_libc=false

dnl Check for usual libc functions
AC_CHECK_FUNCS([gettimeofday isatty sigrelse getpwuid_r memalign posix_memalign if_nametoindex getenv putenv setenv ctime_r daemon fork lstat posix_fadvise posix_madvise uselocale])
AC_CHECK_FUNCS(fcntl)
AC_REPLACE_FUNCS([asprintf atof atoll getcwd getpid gmtime_r lldiv localtime_r rewind strcasecmp strcasestr strdup strlcpy strncasecmp strndup strnlen strsep strtof strtok_r strtoll vasprintf swab tdestroy])
AC_CHECK_FUNCS([stricmp strnicmp])
AC_CHECK_FUNCS(fdatasync,,
  [AC_DEFINE(fdatasync, fsync, [Alias fdatasync() to fsync() if missing.])
])

dnl Check for non-standard system calls
AC_CHECK_FUNCS([vmsplice eventfd fstatfs accept4])

AH_BOTTOM([#include <vlc_fixups.h>])

AC_CHECK_FUNCS(mmap, [VLC_ADD_PLUGIN([access_mmap])])

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

SOCKET_LIBS=""
AC_CHECK_FUNCS(connect,,[
  AC_CHECK_LIB(socket,connect,[
    VLC_ADD_LIBS([libvlccore cdda cddax],-lsocket)
    SOCKET_LIBS="-lsocket"
  ])
])
AC_SUBST(SOCKET_LIBS)

AC_CHECK_FUNCS(send,,[
  AC_CHECK_LIB(socket,send,[
    VLC_ADD_LIBS([access_http access_mms access_udp access_tcp access_ftp access_rtmp sap access_output_udp access_output_rtmp stream_out_standard growl_udp remoteosd],[-lsocket])
  ])
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
     # include <winsock2.h>
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
AC_SEARCH_LIBS([getaddrinfo], [nsl], [AC_DEFINE(HAVE_GETADDRINFO)],, [${SOCKET_LIBS}])

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
    VLC_ADD_LIBS([libvlccore],[-lresolv])
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
     VLC_ADD_LIBS([libvlccore],[-lgnugetopt])],
    [need_getopt=:])])
fi
AM_CONDITIONAL(BUILD_GETOPT, ${need_getopt})

if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
AC_TYPE_SIGNAL
AC_CHECK_LIB(m,cos,[
  VLC_ADD_LIBS([adjust wave ripple psychedelic gradient a52tofloat32 dtstofloat32 x264 goom visual panoramix rotate noise grain scene kate flac lua chorus_flanger],[-lm])
])
AC_CHECK_LIB(m,pow,[
  VLC_ADD_LIBS([avcodec avformat swscale postproc ffmpegaltivec i420_rgb faad twolame equalizer spatializer param_eq libvlccore freetype mod mpc dmo quicktime realaudio realvideo opengl],[-lm])
])
AC_CHECK_LIB(m,sqrt,[
  VLC_ADD_LIBS([headphone_channel_mixer normvol audiobargraph_a speex mono colorthres extract],[-lm])
])
AC_CHECK_LIB(m,ceil,[
  VLC_ADD_LIBS([hotkeys mosaic swscale_omap],[-lm])
])
AC_CHECK_LIB(m,exp,[
  VLC_ADD_LIBS([gaussianblur],[-lm])
])
AC_CHECK_LIB(m,round,[
  VLC_ADD_LIBS([dbus],[-lm])
])
AC_CHECK_LIB(m,sqrtf,[
  VLC_ADD_LIBS([x264],[-lm])
])
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

AC_CHECK_LIB(m,lrintf, [
  AC_DEFINE(HAVE_LRINTF, 1, [Define to 1 if you have the lrintf function])
  VLC_ADD_LIBS([skins2],[-lm])
])

dnl Check for dynamic plugins
ac_cv_have_plugins=no

# HP-UX style
if test "${ac_cv_have_plugins}" = "no"; then
  AC_CHECK_HEADERS(dl.h)
  ac_cv_my_have_shl_load=no
  AC_CHECK_FUNC(shl_load,
   [ac_cv_my_have_shl_load=yes,
    AC_CHECK_LIB(dld, shl_load,
     [ac_cv_my_have_shl_load=yes
      VLC_ADD_LIBS([libvlccore],[-ldld])])])
  if test "${ac_cv_my_have_shl_load}" = "yes"; then
    AC_DEFINE(HAVE_DL_SHL_LOAD, 1, [Define if you have the shl_load API])
    ac_cv_have_plugins=yes
  fi
fi

# Win32 style
if test "${ac_cv_have_plugins}" = "no"; then
  if test "${SYS}" = "mingw32" ; then
    AC_CHECK_LIB(kernel32, main,
     [VLC_ADD_LIBS([libvlccore],[-lkernel32])
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
if test "${ac_cv_have_plugins}" = "no" -o "${SYS}" = "darwin"; then
  AC_CHECK_HEADERS(dlfcn.h sys/dl.h)
  ac_cv_my_have_dlopen=no
  AC_CHECK_FUNC(dlopen,
    ac_cv_my_have_dlopen=yes,
    AC_CHECK_LIB(dl, dlopen,
      ac_cv_my_have_dlopen=yes
      VLC_ADD_LIBS([libvlccore realaudio realvideo lua],[-ldl]),
      AC_CHECK_LIB(svld, dlopen,
        ac_cv_my_have_dlopen=yes
        VLC_ADD_LIBS([libvlccore],[-lsvld]))))
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

VLC_ADD_LIBS([libvlccore libvlc plugin],[${THREAD_LIB}])

AC_CHECK_LIB(rt, clock_nanosleep, [
  VLC_ADD_LIBS([libvlccore],[-lrt])
  AC_DEFINE(HAVE_CLOCK_NANOSLEEP, 1, [Define to 1 if you have clock_nanosleep.])
], [
  dnl HP/UX port
  AC_CHECK_LIB(rt,sem_init, [VLC_ADD_LIBS([libvlccore],[-lrt])])
])

have_nanosleep=false
AC_CHECK_FUNCS(nanosleep,have_nanosleep=:,[
  AC_CHECK_LIB(rt,nanosleep,
    [VLC_ADD_LIBS([libvlccore],[-lrt]) have_nanosleep=:],
    [AC_CHECK_LIB(posix4,nanosleep,
        [VLC_ADD_LIBS([libvlccore],[-lposix4]) have_nanosleep=:])]
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
AC_CHECK_HEADERS(getopt.h strings.h locale.h xlocale.h)
AC_CHECK_HEADERS(fcntl.h sys/types.h sys/time.h sys/times.h sys/ioctl.h sys/stat.h sys/mount.h)
AC_CHECK_HEADERS([arpa/inet.h netinet/in.h netinet/udplite.h sys/eventfd.h])
AC_CHECK_HEADERS([net/if.h], [], [],
  [
    #include <sys/types.h>
    #include <sys/socket.h>
  ])
if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
AC_CHECK_HEADERS(machine/param.h sys/shm.h)
AC_CHECK_HEADERS([linux/version.h linux/dccp.h scsi/scsi.h linux/magic.h])
AC_CHECK_HEADERS(syslog.h)
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

AC_HEADER_TIME

dnl LP64 adn LLP64 architectures had better define ssize_t by themselves...
AH_TEMPLATE(ssize_t, [Define to `int' if <stddef.h> does not define.])
AC_CHECK_TYPE(ssize_t,, [
  AC_DEFINE(ssize_t, int)
])

dnl Check for poll
AC_SEARCH_LIBS(poll, [poll], [AC_DEFINE(HAVE_POLL, 1, [Define to 1 if the OS is usabl... err, has poll().])], [
if test ${SYS} != "mingw32" -a ${SYS} != "mingwce"
then
    AC_MSG_WARN([Your platform does not support poll(). VLC has it's own poll() implementation, but it is only intended to be used on Windows. VLC might crash or be insecure when you see this message. Either switch to an OS with a proper poll() implementation, or implement one for your OS in VLC])
fi
])

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
AC_CHECK_HEADERS(pthread.h)
fi # end "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"

dnl It seems that autoconf detects pkg-config only during the first
dnl PKG_CHECK_MODULES from configure.ac - which makes sense. But in our case,
dnl it is nested within a conditional block, so it was not working right.
dnl Make PKG_CONFIG_PATH precious so that it appears in the help and get saved
AC_ARG_VAR(PKG_CONFIG_PATH,
	   [Paths where to find .pc not at the default location])
PKG_PROG_PKG_CONFIG()

dnl On some OS we need static linking 
AS_IF([test "${SYS}" = "mingw32" -o "${SYS}" = "mingwce" -o "${SYS}" = "darwin" ],[
       PKG_CONFIG="${PKG_CONFIG} --static"
    ])


dnl
dnl Check for zlib.h and -lz along with system -lminizip if available
dnl 
AC_CHECK_HEADERS(zlib.h, [ have_zlib=yes ], [ have_zlib=no ])
AM_CONDITIONAL(HAVE_ZLIB, [ test "${have_zlib}" = "yes" ])
if test "${have_zlib}" = "yes"
then
  VLC_ADD_LIBS([access_http gme mp4 skins2 sap mkv unzip zip],[-lz])
  PKG_CHECK_MODULES([MINIZIP], [minizip] , [ have_minizip=yes ], [
    AC_CHECK_HEADERS([unzip.h], [ 
      have_minizip=yes
      MINIZIP_LIBS="-lminizip -lz"
    ], [
      have_minizip=no
      MINIZIP_CFLAGS="-I\\\${top_srcdir}/libs/unzip"
      MINIZIP_LIBS="\\\${top_builddir}/libs/unzip/libunzip.la"
    ])
  ])
  VLC_ADD_CPPFLAGS([skins2],[$MINIZIP_CFLAGS])
  VLC_ADD_CFLAGS([zip],[$MINIZIP_CFLAGS])
  VLC_ADD_LIBS([skins2 zip],[$MINIZIP_LIBS])
  VLC_ADD_PLUGIN([unzip])
  VLC_ADD_PLUGIN([zip])
fi
AM_CONDITIONAL(HAVE_MINIZIP, [ test "${have_minizip}" = "yes" ])


dnl Check for dbus
AC_ARG_ENABLE(dbus,
  [  --enable-dbus           Linux D-BUS message bus system (default enabled)])
case "${SYS}" in
    linux*|*bsd*)
if test "${enable_dbus}" != "no" -a "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"
then
  dnl api stable dbus
  PKG_CHECK_MODULES(DBUS, dbus-1 >= 1.0.0,
    [ AC_DEFINE( HAVE_DBUS, 1, [Define if you have the D-BUS library] )
      VLC_ADD_LIBS([libvlccore],[$DBUS_LIBS])
      VLC_ADD_CFLAGS([libvlccore],[$DBUS_CFLAGS])
      dnl Check for dbus control interface
        AC_ARG_ENABLE(dbus-control, [  --disable-dbus-control  D-BUS control interface (default enabled)])
        if test "${enable_dbus_control}" != "no"
        then
          VLC_ADD_PLUGIN([dbus])
          VLC_ADD_LIBS([dbus],[$DBUS_LIBS])
          VLC_ADD_CFLAGS([dbus],[$DBUS_CFLAGS])
        fi
      dnl Check for Telepathy
        AC_ARG_ENABLE(telepathy,
          AS_HELP_STRING([--enable-telepathy],[Telepathy Presence plugin through DBus(default enabled)]))
        if test "${enable_telepathy}" != "no"; then
          VLC_ADD_PLUGIN([telepathy])
          VLC_ADD_LIBS([telepathy],[$DBUS_LIBS])
          VLC_ADD_CFLAGS([telepathy],[$DBUS_CFLAGS])
        fi
        dnl Power Management Inhibiter
        VLC_ADD_PLUGIN([inhibit])
        VLC_ADD_LIBS([inhibit],[$DBUS_LIBS])
        VLC_ADD_CFLAGS([inhibit],[$DBUS_CFLAGS])
    ],
    [AC_MSG_ERROR([Couldn't find DBus >= 1.0.0, install libdbus-dev ?])]
  )
fi
;;
esac

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

RDC_PROG_CC_WFLAGS([all extra sign-compare undef pointer-arith bad-function-cast write-strings missing-prototypes volatile-register-var error-implicit-function-declaration])
RDC_PROG_CC_FLAGS([-pipe])

AC_ARG_ENABLE(warnings-as-error,
      AS_HELP_STRING([--enable-warnings-as-error],[Treat acceptable warnings as error (default disabled)]),, [enable_warnings_as_error="no"])
AS_IF([test "${enable_warnings_as_error}" = "yes"], [
    RDC_PROG_CC_WERRORFLAGS([unused-function unused-label unused-value unused-parameter unused-variable unused \
     missing-braces nonnull parentheses uninitialized address enum-compare undef comment switch \
     bad-function-cast implicit sequence-point strict-aliasing array-bounds missing-parameter-type \
     overflow int-to-pointer-cast pointer-to-int-cast div-by-zero overflow ignored])
])
AC_SUBST(CFLAGS_WERROR)

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

AC_CACHE_CHECK([if \$CC accepts -O4],
    [ac_cv_c_o4],
    [CFLAGS="${CFLAGS_save} -O4"
     AC_TRY_COMPILE([],,ac_cv_c_o4=yes, ac_cv_c_o4=no)])
if test "${ac_cv_c_o4}" != "no" -a "x${enable_debug}" = "xno"; then
    CFLAGS_OPTIM_SPEED="${CFLAGS_OPTIM_SPEED} -O4"
else
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
 if test "${SYS}" != "darwin"; then
    CFLAGS_OPTIM_NODEBUG="${CFLAGS_OPTIM_NODEBUG} -fomit-frame-pointer"
 else
    dnl On darwin we explicitely disable it.
    CFLAGS_OPTIM_NODEBUG="${CFLAGS_OPTIM_NODEBUG} -fno-omit-frame-pointer"
 fi
fi

dnl Check for Darwin plugin linking flags
AC_CACHE_CHECK([if \$CC accepts -bundle -undefined error],
    [ac_cv_ld_darwin],
    [CFLAGS="${CFLAGS_save} -bundle -undefined error"
     AC_TRY_COMPILE([],,ac_cv_ld_darwin=yes, ac_cv_ld_darwin=no)])
if test "${ac_cv_ld_darwin}" != "no"; then
    VLC_ADD_LDFLAGS([plugin],[-bundle -undefined error])
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
  *)
    ARCH="${host_cpu}"
    ;;
esac

dnl Check for backtrace() support
AC_CHECK_HEADERS(execinfo.h)
AC_CHECK_FUNCS(backtrace)

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
dnl Video filters:
VLC_ADD_PLUGIN([deinterlace])
VLC_ADD_PLUGIN([yuvp])
VLC_ADD_PLUGIN([invert])
VLC_ADD_PLUGIN([mirror])
VLC_ADD_PLUGIN([adjust])
VLC_ADD_PLUGIN([transform])
VLC_ADD_PLUGIN([wave])
VLC_ADD_PLUGIN([ripple])
VLC_ADD_PLUGIN([psychedelic])
VLC_ADD_PLUGIN([gradient])
VLC_ADD_PLUGIN([motionblur])
VLC_ADD_PLUGIN([rv32])
VLC_ADD_PLUGIN([rotate])
VLC_ADD_PLUGIN([noise])
VLC_ADD_PLUGIN([grain])
VLC_ADD_PLUGIN([extract])
VLC_ADD_PLUGIN([sharpen])
VLC_ADD_PLUGIN([croppadd])
VLC_ADD_PLUGIN([canvas])
VLC_ADD_PLUGIN([blendbench])
VLC_ADD_PLUGIN([blend])
VLC_ADD_PLUGIN([scale])
VLC_ADD_PLUGIN([vmem])
VLC_ADD_PLUGIN([logo])
VLC_ADD_PLUGIN([audiobargraph_v])
VLC_ADD_PLUGIN([magnify])
VLC_ADD_PLUGIN([puzzle])
VLC_ADD_PLUGIN([colorthres])
VLC_ADD_PLUGIN([scene])
VLC_ADD_PLUGIN([yuv])

ALIASES="${ALIASES} cvlc"

dnl
dnl default modules that need g++
dnl
AC_LANG_PUSH(C++)
VLC_ADD_PLUGIN([spatializer])
AC_LANG_POP(C++)

if test "${SYS}" != "mingwce"; then
  VLC_ADD_PLUGIN([gestures])
  VLC_ADD_PLUGIN([rc])
  VLC_ADD_PLUGIN([hotkeys])
  VLC_ADD_PLUGIN([showintf])
  VLC_ADD_PLUGIN([marq])
  VLC_ADD_PLUGIN([podcast])
  VLC_ADD_PLUGIN([shout])
  VLC_ADD_PLUGIN([sap])
  VLC_ADD_PLUGIN([mediadirs])
  VLC_ADD_PLUGIN([fake])
  VLC_ADD_PLUGIN([folder])
  VLC_ADD_PLUGIN([rss])
  VLC_ADD_PLUGIN([mosaic])
  VLC_ADD_PLUGIN([wall])
  VLC_ADD_PLUGIN([motiondetect])
  VLC_ADD_PLUGIN([clone])
  VLC_ADD_PLUGIN([crop])
  VLC_ADD_PLUGIN([erase])
  VLC_ADD_PLUGIN([bluescreen])
  VLC_ADD_PLUGIN([alphamask])
  VLC_ADD_PLUGIN([gaussianblur])
  VLC_ADD_PLUGIN([i420_yuy2])
  VLC_ADD_PLUGIN([i422_yuy2])
  VLC_ADD_PLUGIN([i420_ymga])
  VLC_ADD_PLUGIN([i422_i420])
  VLC_ADD_PLUGIN([yuy2_i422])
  VLC_ADD_PLUGIN([yuy2_i420])
  VLC_ADD_PLUGIN([chain])
  VLC_ADD_PLUGIN([aout_file])
  VLC_ADD_PLUGIN([linear_resampler])
  VLC_ADD_PLUGIN([bandlimited_resampler])
  VLC_ADD_PLUGIN([float32_mixer])
  VLC_ADD_PLUGIN([spdif_mixer])
  VLC_ADD_PLUGIN([simple_channel_mixer])
  VLC_ADD_PLUGIN([dolby_surround_decoder])
  VLC_ADD_PLUGIN([headphone_channel_mixer])
  VLC_ADD_PLUGIN([normvol])
  VLC_ADD_PLUGIN([audiobargraph_a])
  VLC_ADD_PLUGIN([equalizer])
  VLC_ADD_PLUGIN([param_eq])
  VLC_ADD_PLUGIN([scaletempo])
  VLC_ADD_PLUGIN([a52tospdif])
  VLC_ADD_PLUGIN([dtstospdif])
  ALIASES="${ALIASES} rvlc"
fi

if test "${SYS}" = "mingwce"; then
  VLC_ADD_PLUGIN([chain])
  VLC_ADD_PLUGIN([aout_file])
  VLC_ADD_PLUGIN([linear_resampler])
  VLC_ADD_PLUGIN([bandlimited_resampler])
  VLC_ADD_PLUGIN([float32_mixer])
  VLC_ADD_PLUGIN([spdif_mixer])
  VLC_ADD_PLUGIN([simple_channel_mixer])
  VLC_ADD_PLUGIN([headphone_channel_mixer])
  VLC_ADD_PLUGIN([normvol])
  VLC_ADD_PLUGIN([audiobargraph_a])
  VLC_ADD_PLUGIN([a52tospdif])
  VLC_ADD_PLUGIN([dtstospdif])
  VLC_ADD_PLUGIN([i420_yuy2])
  VLC_ADD_PLUGIN([i422_yuy2])
  VLC_ADD_PLUGIN([i420_ymga])
  VLC_ADD_PLUGIN([i422_i420])
  VLC_ADD_PLUGIN([yuy2_i422])
  VLC_ADD_PLUGIN([yuy2_i420])
fi

dnl
dnl Some plugins aren't useful on some platforms
dnl
if test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"; then
    VLC_ADD_PLUGIN([screensaver])
    VLC_ADD_PLUGIN([motion])
    VLC_ADD_PLUGIN([dynamicoverlay])
    VLC_ADD_PLUGIN([signals])
elif test "${SYS}" != "mingwce"; then
    VLC_ADD_PLUGIN([ntservice])
    VLC_ADD_PLUGIN([access_smb])
    VLC_ADD_PLUGIN([dmo])
    VLC_ADD_PLUGIN([msn])
    VLC_ADD_LIBS([dmo],[-lole32 -luuid])
    VLC_ADD_PLUGIN([globalhotkeys])
else
    VLC_ADD_PLUGIN([win32text])
fi
if test "${SYS}" = "darwin"; then
    VLC_ADD_PLUGIN([quartztext])
    VLC_ADD_LDFLAGS([quartztext],[-Wl,-framework,ApplicationServices])
fi

dnl
dnl  Accelerated modules
dnl
AC_ARG_WITH(,[])
AC_ARG_WITH(,[Optimization options:])

dnl  Check for fully working MMX intrinsics
dnl  We need support for -mmmx, we need <mmintrin.h>, and we also need a
dnl  working compiler (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=23963)
AC_ARG_ENABLE(mmx,
[  --disable-mmx           disable MMX optimizations (default auto)],,[
  case "${host_cpu}" in
    i?86|x86_64)
      enable_mmx="yes"
      ;;
    *)
      enable_mmx="no"
      ;;
  esac
])
AS_IF([test "${enable_mmx}" != "no"], [
  ARCH="${ARCH} mmx"

  AC_CACHE_CHECK([if $CC groks MMX intrinsics],
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
  AS_IF([test "${ac_cv_c_mmx_intrinsics}" != "no"], [
    AC_DEFINE(HAVE_MMX_INTRINSICS, 1,
              [Define to 1 if MMX intrinsics are available.])
    VLC_ADD_CFLAGS([memcpymmx i420_rgb_mmx i420_yuy2_mmx i422_yuy2_mmx i420_ymga_mmx],[-mmmx])
  ])

  AC_CACHE_CHECK([if $CC groks MMX inline assembly],
    [ac_cv_mmx_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("packuswb %%mm1,%%mm2"::"r"(p));],
                    ac_cv_mmx_inline=yes, ac_cv_mmx_inline=no)])
  AS_IF([test "${ac_cv_mmx_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_MMX, 1,
              [Define to 1 inline MMX assembly is available.])
    VLC_ADD_PLUGIN([memcpymmx])
    VLC_ADD_PLUGIN([i420_rgb_mmx])
    VLC_ADD_PLUGIN([i420_yuy2_mmx])
    VLC_ADD_PLUGIN([i422_yuy2_mmx])
    VLC_ADD_PLUGIN([i420_ymga_mmx])
  ])

  AC_CACHE_CHECK([if $CC groks MMX EXT inline assembly],
    [ac_cv_mmxext_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("maskmovq %%mm1,%%mm2"::"r"(p));],
                    ac_cv_mmxext_inline=yes, ac_cv_mmxext_inline=no)])
  AS_IF([test "${ac_cv_mmxext_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_MMXEXT, 1,
              [Define to 1 if MMX EXT inline assembly is available.])
    VLC_ADD_PLUGIN([memcpymmxext])
  ])
])

dnl  Check for fully workin SSE2 intrinsics
dnl  We need support for -mmmx, we need <emmintrin.h>, and we also need a
dnl  working compiler (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=23963)
AC_ARG_ENABLE(sse,
[  --disable-sse           disable SSE (1, 2, 3 and 4) optimizations (default auto)]
,, [
  case "${host_cpu}" in
    i686|x86_64)
      enable_sse=yes
      ;;
    *)
      enable_sse=no
      ;;
  esac
])
AS_IF([test "${enable_sse}" != "no"], [
  ARCH="${ARCH} sse sse2"

  AC_CACHE_CHECK([if $CC groks SSE2 intrinsics],
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
  AS_IF([test "${ac_cv_c_sse2_intrinsics}" != "no"], [
    AC_DEFINE(HAVE_SSE2_INTRINSICS, 1,
              [Define to 1 if SSE2 intrinsics are available.])
    VLC_ADD_CFLAGS([i420_rgb_sse2 i420_yuy2_sse2 i422_yuy2_sse2],[-msse2])
  ])

  AC_CACHE_CHECK([if $CC groks SSE inline assembly],
    [ac_cv_sse_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("xorps %%xmm1,%%xmm2"::"r"(p));],
                    ac_cv_sse_inline=yes, ac_cv_sse_inline=no)])
  AS_IF([test "${ac_cv_sse_inline}" != "no" -a "${SYS}" != "solaris"], [
    AC_DEFINE(CAN_COMPILE_SSE, 1,
              [Define to 1 if SSE inline assembly is available.])
  ])

  AC_CACHE_CHECK([if $CC groks SSE2 inline assembly],
    [ac_cv_sse2_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("punpckhqdq %%xmm1,%%xmm2"::"r"(p));],
                    ac_cv_sse2_inline=yes, ac_cv_sse2_inline=no)])
  AS_IF([test "${ac_cv_sse2_inline}" != "no" -a "${SYS}" != "solaris"], [
    AC_DEFINE(CAN_COMPILE_SSE2, 1,
              [Define to 1 if SSE2 inline assembly is available.])
    VLC_ADD_PLUGIN([i420_rgb_sse2])
    VLC_ADD_PLUGIN([i420_yuy2_sse2])
    VLC_ADD_PLUGIN([i422_yuy2_sse2])
  ])

  # SSE3
  AC_CACHE_CHECK([if $CC groks SSE3 inline assembly],
    [ac_cv_sse3_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("movsldup %%xmm1,%%xmm0"::"r"(p));],
                    ac_cv_sse3_inline=yes, ac_cv_sse3_inline=no)])
  AS_IF([test "${ac_cv_sse3_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_SSE3, 1,
              [Define to 1 if SSE3 inline assembly is available.]) ])
  # SSSE3
  AC_CACHE_CHECK([if $CC groks SSSE3 inline assembly],
    [ac_cv_ssse3_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("pabsw %%xmm0,%%xmm0"::"r"(p));],
                    ac_cv_ssse3_inline=yes, ac_cv_ssse3_inline=no)])
  AS_IF([test "${ac_cv_ssse3_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_SSSE3, 1,
              [Define to 1 if SSSE3 inline assembly is available.]) ])

  # SSE4.1
  AC_CACHE_CHECK([if $CC groks SSE4.1 inline assembly],
    [ac_cv_sse4_1_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("pmaxsb %%xmm1,%%xmm0"::"r"(p));],
                    ac_cv_sse4_1_inline=yes, ac_cv_sse4_1_inline=no)])
  AS_IF([test "${ac_cv_sse4_1_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_SSE4_1, 1,
              [Define to 1 if SSE4_1 inline assembly is available.]) ])

  # SSE4.2
  AC_CACHE_CHECK([if $CC groks SSE4.2 inline assembly],
    [ac_cv_sse4_2_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("pcmpgtq %%xmm1,%%xmm0"::"r"(p));],
                    ac_cv_sse4_2_inline=yes, ac_cv_sse4_2_inline=no)])
  AS_IF([test "${ac_cv_sse4_2_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_SSE4_2, 1,
              [Define to 1 if SSE4_2 inline assembly is available.]) ])

  # SSE4A
  AC_CACHE_CHECK([if $CC groks SSE4A inline assembly],
    [ac_cv_sse4a_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("insertq %%xmm1,%%xmm0"::"r"(p));],
                    ac_cv_sse4a_inline=yes, ac_cv_sse4a_inline=no)])
  AS_IF([test "${ac_cv_sse4a_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_SSE4A, 1,
              [Define to 1 if SSE4A inline assembly is available.]) ])
])

AC_CACHE_CHECK([if $CC groks 3D Now! inline assembly],
    [ac_cv_3dnow_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[void *p;asm volatile("pfadd %%mm1,%%mm2"::"r"(p));],
                    ac_cv_3dnow_inline=yes, ac_cv_3dnow_inline=no)])
AS_IF([test "${ac_cv_3dnow_inline}" != "no"], [
  AC_DEFINE(CAN_COMPILE_3DNOW, 1,
            [Define to 1 if 3D Now! inline assembly is available.])
  VLC_ADD_PLUGIN([memcpy3dn])
])


AC_ARG_ENABLE(neon,
[ --disable-neon           disable NEON optimizations (default auto)],, [
  AS_IF([test "${host_cpu}" = "arm"], [enable_neon="yes"] ,[enable_neon="no"])
])
AS_IF([test "${enable_neon}" != "no"], [
  AC_CACHE_CHECK([if $CC groks NEON inline assembly], [ac_cv_neon_inline], [
    AC_COMPILE_IFELSE([
      AC_LANG_PROGRAM(,[[asm volatile("vqmovun.s64 d0, q1":::"d0");]])
    ], [
      ac_cv_neon_inline="yes"
    ], [
      ac_cv_neon_inline="no"
    ])
  ])
])
AM_CONDITIONAL(HAVE_NEON, [test "${ac_cv_neon_inline}" = "yes"])

AC_ARG_ENABLE(altivec,
[  --disable-altivec       disable AltiVec optimizations (default auto)],, [
  AS_IF([test "${host_cpu}" = "powerpc"],
        [enable_altivec=yes], [enable_altivec=no])
])
AS_IF([test "${enable_altivec}" = "yes"], [
  ARCH="${ARCH} altivec";
  AC_CACHE_CHECK([if $CC groks AltiVec inline assembly],
    [ac_cv_altivec_inline],
    [CFLAGS="${CFLAGS_save}"
     AC_TRY_COMPILE(,[asm volatile("vperm 0,1,2,3");],
         ac_cv_altivec_inline="yes",
         [CFLAGS="${CFLAGS_save} -Wa,-m7400"
          AC_TRY_COMPILE(,[asm volatile("vperm 0,1,2,3");],
            [ac_cv_altivec_inline="-Wa,-m7400"],
            ac_cv_altivec_inline=no)
         ])])
  AS_IF([test "${ac_cv_altivec_inline}" != "no"], [
    AC_DEFINE(CAN_COMPILE_ALTIVEC, 1,
              [Define to 1 if AltiVec inline assembly is available.])
    AS_IF([test "${ac_cv_altivec_inline}" != "yes"], [
      VLC_ADD_CFLAGS([idctaltivec],[${ac_cv_altivec_inline}])
      VLC_ADD_CFLAGS([motionaltivec],[${ac_cv_altivec_inline}])
      VLC_ADD_CFLAGS([memcpyaltivec],[${ac_cv_altivec_inline}])
      VLC_ADD_CFLAGS([i420_yuy2_altivec],[${ac_cv_altivec_inline}])
      VLC_ADD_CFLAGS([libvlccore],[${ac_cv_altivec_inline}])
    ])
    VLC_ADD_PLUGIN([memcpyaltivec])
    VLC_ADD_PLUGIN([i420_yuy2_altivec])
  ])

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
  AS_IF([test "${ac_cv_c_altivec}" != "no"], [
    CPPFLAGS="${CPPFLAGS_save} ${ac_cv_c_altivec}"
  ])

  AC_CHECK_HEADERS(altivec.h)
  CPPFLAGS="${CPPFLAGS_save}"

  AS_IF([test "${ac_cv_c_altivec}" != "no"], [
    AC_DEFINE(CAN_COMPILE_C_ALTIVEC, 1,
              [Define to 1 if C AltiVec extensions are available.])
    VLC_ADD_CFLAGS([libvlccore],[${ac_cv_c_altivec}])
    VLC_ADD_CFLAGS([idctaltivec motionaltivec],[${ac_cv_c_altivec}])
    VLC_ADD_CFLAGS([i420_yuy2_altivec memcpyaltivec deinterlace],[${ac_cv_c_altivec} ${ac_cv_c_altivec_abi}])
    VLC_ADD_PLUGIN([memcpyaltivec])
    VLC_ADD_PLUGIN([i420_yuy2_altivec])
  ])

  AC_CACHE_CHECK([if linker needs -framework vecLib],
    [ac_cv_ld_altivec],
    [LDFLAGS="${LDFLAGS_vlc} -Wl,-framework,vecLib"
     AC_TRY_LINK([],,ac_cv_ld_altivec=yes,ac_cv_ld_altivec=no)
     LDFLAGS="${LDFLAGS_save}"
    ])
  AS_IF([test "${ac_cv_ld_altivec}" != "no"], [
    VLC_ADD_LDFLAGS([libvlccore idctaltivec motionaltivec memcpyaltivec],[-Wl,-framework,vecLib])
  ])
])

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
        CFLAGS_TUNING="-march=prescott -mtune=generic"
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
dnl  Debugging mode
dnl
AC_ARG_ENABLE(debug,
[  --enable-debug          debug mode (default disabled)])
test "${enable_debug}" != "yes" && enable_debug="no"
AH_TEMPLATE(NDEBUG,
            [Define to 1 if debug code should NOT be compiled])
AS_IF([test "x${enable_debug}" = "xno"], [
  AC_DEFINE(NDEBUG)
], [
  AC_CHECK_HEADERS([valgrind/valgrind.h])
])

dnl
dnl Allow runing as root (usefull for people runing on embedded platforms)
dnl
AC_ARG_ENABLE(run-as-root,
[  --enable-run-as-root    allow runing VLC as root (default disabled)])
AS_IF([test "${enable_run_as_root}" = "yes"],[
    AC_DEFINE(ALLOW_RUN_AS_ROOT, 1,
              [Define to 1 to allow runing VLC as root (uid 0).])
])

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
AS_IF([test "${enable_sout}" != "no"], [
  AC_DEFINE(ENABLE_SOUT, 1, [Define to 1 for stream output support.])
])
AM_CONDITIONAL(ENABLE_SOUT, [test "${enable_sout}" != "no"])

dnl Lua modules
AC_ARG_ENABLE(lua,
  AS_HELP_STRING([--enable-lua],[lua playlist, metafetcher and interface
		  plugins (default enabled)]))
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
                AS_IF([test "x${enable_lua}" = "xyes"],
                  [AC_MSG_ERROR([lua >= 5.1 not found!])],
                  [AC_MSG_WARN([lua >= 5.1 not found!])])
              ], [-lm])
          )
        )
      ])
    ])
  if test "x${have_lua}" = "xyes" ;  then
     VLC_ADD_LIBS([lua],[$LUA_LIBS])
     VLC_ADD_CFLAGS([lua],[$LUA_CFLAGS])
  fi
fi
AM_CONDITIONAL(BUILD_LUA, [test "${have_lua}" = "yes"])

dnl
dnl HTTP daemon
dnl
AC_ARG_ENABLE(httpd,
  [  --enable-httpd          HTTP daemon (default enabled)])
if test "${enable_httpd}" != "no"
then
  VLC_ADD_PLUGIN([http])
  AC_DEFINE(ENABLE_HTTPD, 1, Define if you want the HTTP dameon support)
fi
AM_CONDITIONAL(BUILD_HTTPD, [test "${enable_httpd}" != "no"])

dnl
dnl libproxy support
dnl
AC_ARG_ENABLE(libproxy,
  [  --enable-libproxy       libproxy support (default auto)])
AS_IF([test "${enable_libproxy}" != "no"], [
  PKG_CHECK_MODULES(LIBPROXY, libproxy-1.0, [
    AC_DEFINE(HAVE_LIBPROXY, 1, [Define if libproxy is available])
    VLC_ADD_CFLAGS([access_http],[$LIBPROXY_CFLAGS])
    VLC_ADD_LIBS([access_http],[$LIBPROXY_LIBS])
  ], [
    AS_IF([test "x${enable_libproxy}" != "x"], [
      AC_MSG_ERROR([libproxy could not be found on your system])
    ])
  ])
])


dnl
dnl VideoLAN manager
dnl
AC_ARG_ENABLE(vlm,
  [  --enable-vlm            VideoLAN manager (default enabled)],,
  [enable_vlm="${enable_sout}"])
AS_IF([test "${enable_vlm}" != "no"], [
  AS_IF([test "${enable_sout}" = "no"], [
    AC_MSG_ERROR([VLM requires the stream output. Do not use --disable-sout.])
  ])
  AC_DEFINE(ENABLE_VLM, 1, [Define if you want the VideoLAN manager support])
  VLC_ADD_PLUGIN([telnet])
])
AM_CONDITIONAL([ENABLE_VLM], [test "${enable_vlm}" != "no"])

dnl
dnl Growl notification plugin
dnl
AC_ARG_ENABLE(growl,
  [  --enable-growl          growl notification plugin (default disabled)],,
  [enable_growl=no])
AS_IF([test "${enable_growl}" != "no"], [
    VLC_ADD_PLUGIN([growl_udp])
    AC_CHECK_HEADERS(Growl/GrowlDefines.h, [
      VLC_ADD_PLUGIN([growl])
      VLC_ADD_LDFLAGS([growl], [-Wl,-framework,Growl,-framework,AppKit])
    ])
  ]
)

dnl
dnl Libnotify notification plugin
dnl
PKG_ENABLE_MODULES_VLC([NOTIFY], [], [libnotify], [libnotify notification], [auto])

dnl
dnl Audioscrobbler plugin
dnl

AC_ARG_ENABLE(audioscrobbler,
  [  --disable-audioscrobbler Last.fm submission plugin (default enabled)])
AS_IF([test "${enable_audioscrobbler}" != "no"], [
        VLC_ADD_PLUGIN([audioscrobbler])
  ])

dnl
dnl Taglibplugin
dnl
AC_ARG_ENABLE(taglib,
  [  --disable-taglib        Taglib support (default enabled) ])
  AS_IF([test "${enable_taglib}" != "no"],[
    PKG_CHECK_MODULES(TAGLIB, taglib >= 1.5,
      [ VLC_ADD_PLUGIN([taglib])
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

    AC_CHECK_HEADERS(liveMedia_version.hh, [
      AC_MSG_CHECKING(for liveMedia version >= 1214895600 )
      AC_EGREP_CPP(yes,
        [#include <liveMedia_version.hh>
         #ifdef LIVEMEDIA_LIBRARY_VERSION_INT
         #if LIVEMEDIA_LIBRARY_VERSION_INT < 1214895600
         yes
         #endif
         #endif],
        [AC_MSG_RESULT([no])
         AC_MSG_ERROR([Your version of liveMedia is too old: you may get a more recent one from http://www.live555.com/liveMedia.
lternatively you can use --disable-live555 to disable the liveMedia plugin.])
      ],[
	 AC_MSG_RESULT([yes])
	 ])
    ])
     other_libs="-lgroupsock -lBasicUsageEnvironment -lUsageEnvironment"
     other_libs_pic="-lgroupsock_pic -lBasicUsageEnvironment_pic -lUsageEnvironment_pic"
      if test "${SYS}" = "mingw32"; then
        # add ws2_32 for closesocket, select, recv
        other_libs="$other_libs -lws2_32"
      elif test "${SYS}" = "mingwce"; then
        # add ws2 for closesocket, select, recv
        other_libs="$other_libs -lws2"
      fi
    dnl We need to check for pic because live555 don't provide shared libs
    dnl and we want to build a plugins so we need -fPIC on some arch.
    AC_CHECK_HEADERS(liveMedia.hh, [
      VLC_ADD_CXXFLAGS([live555], [${CPPFLAGS_live555}])
        AC_CHECK_LIB(liveMedia_pic, main, [
          VLC_ADD_PLUGIN([live555])
         VLC_ADD_LIBS([live555], [-lliveMedia_pic ${other_libs_pic}])
	 ],[
        AC_CHECK_LIB(liveMedia, main, [
          VLC_ADD_PLUGIN([live555])
          VLC_ADD_LIBS([live555], [-lliveMedia ${other_libs}])
        ],[],[${other_libs}]) ],[${other_libs_pic}])
    ],[
       AC_MSG_WARN([The development files for liveMedia (live555) can't be found])
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

      AC_CHECK_HEADERS(${real_live555_tree}/liveMedia/include/liveMedia_version.hh,[
        AC_MSG_CHECKING(for liveMedia version >= 1214895600 )
        AC_EGREP_CPP(yes,
          [#include "${real_live555_tree}/liveMedia/include/liveMedia_version.hh"
           #ifdef LIVEMEDIA_LIBRARY_VERSION_INT
           #if LIVEMEDIA_LIBRARY_VERSION_INT < 1214895600
           yes
           #endif
           #endif],
          [AC_MSG_RESULT([no])
           AC_MSG_ERROR([Your version of liveMedia is too old: you may get a more recent one from http://www.live555.com/liveMedia.
lternatively you can use --disable-live555 to disable the liveMedia plugin.])
        ],[
	   AC_MSG_RESULT([yes])
	   ])
      ])

      VLC_ADD_PLUGIN([live555])

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
dnl - special access module for dc1394 input
dnl - dv module: digital video module check for libraw1394
dnl
PKG_ENABLE_MODULES_VLC([DC1394], [], [libraw1394 >= 2.0.1 libdc1394-2 >= 2.1.0], [dc1394 access module], [auto])
PKG_ENABLE_MODULES_VLC([DV], [access_dv], [libraw1394 >= 2.0.1 libavc1394 >= 0.5.3], [DV input module], [auto])

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
        [ VLC_ADD_PLUGIN([dvdread])
          VLC_ADD_LIBS([dvdread],[-ldvdread ${LIBS_dvdcss}])
        ],[
	  AC_CHECK_HEADERS(libdvdread/dvd_reader.h,
            [ VLC_ADD_PLUGIN([dvdread])
	      VLC_ADD_LIBS([dvdread],[-ldvdread ${LIBS_dvdcss}])
	    ],[
	      if test -n "${enable_dvdread}"
	      then
	      AC_MSG_ERROR([cannot find libdvdread headers])
	     fi
	   ])
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
        VLC_ADD_PLUGIN([dvdread])
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
      VLC_ADD_PLUGIN([dvdread])
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
    VLC_ADD_PLUGIN([dvdnav])
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
  if test "${SYS}" = "mingw32"
  then
    AC_LANG_PUSH(C++)
      AC_CHECK_HEADERS(dshow.h,
      [ VLC_ADD_PLUGIN([dshow])
        VLC_ADD_CXXFLAGS([dshow],[])
        VLC_ADD_LIBS([dshow],[-lole32 -loleaut32 -luuid]) ])
    AC_LANG_POP(C++)
  fi
fi

dnl
dnl  Windows DirectShow BDA access module
dnl
AC_ARG_ENABLE(bda,
  AS_HELP_STRING([--enable-bda],[Win32 DirectShow BDA support (default
		  enabled on Win32)]))
if test "${enable_bda}" != "no"
then
  if test "${SYS}" = "mingw32"
  then
      AC_CHECK_HEADERS(dshow.h,
      [ VLC_ADD_PLUGIN([bda])
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
    if test "${SYS}" = "mingw32"
    then
      test -z "${with_opencv_tree}" && AC_MSG_ERROR([You have to specify --with-opencv-tree])
      AC_MSG_CHECKING(for opencv in ${with_opencv_tree})
      if test -f ${with_opencv_tree}/cv/include/cv.h -a -f ${with_opencv_tree}/cxcore/include/cxcore.h \
        -a -f ${with_opencv_tree}/cvaux/include/cvaux.h -a -f ${with_opencv_tree}/otherlibs/highgui/highgui.h
      then
        AC_MSG_RESULT(yes)
        VLC_ADD_PLUGIN([opencv_wrapper])
        VLC_ADD_LIBS([opencv_wrapper],[-L${with_opencv_tree}/lib -lcv -lcxcore -lcvaux -lhighgui])
        VLC_ADD_CFLAGS([opencv_wrapper],[-I${with_opencv_tree}/cv/include -I${with_opencv_tree}/cxcore/include -I${with_opencv_tree}/cvaux/include -I${with_opencv_tree}/otherlibs/highgui])
        AC_LANG_PUSH(C++)
        VLC_ADD_PLUGIN([opencv_example])
        VLC_ADD_LIBS([opencv_example],[-L${with_opencv_tree}/lib -lcv -lcxcore -lcvaux -lhighgui])
        VLC_ADD_CXXFLAGS([opencv_example],[-I${with_opencv_tree}/cv/include -I${with_opencv_tree}/cxcore/include -I${with_opencv_tree}/cvaux/include -I${with_opencv_tree}/otherlibs/highgui])
        AC_LANG_POP(C++)
      else
        dnl  No opencv could be found, sorry
        AC_MSG_RESULT(no)
        AC_MSG_ERROR([cannot find opencv in ${with_opencv_tree}])
      fi
    else
      PKG_CHECK_MODULES(OPENCV, opencv,
        [
          VLC_ADD_PLUGIN([opencv_example])
          VLC_ADD_PLUGIN([opencv_wrapper])
          VLC_ADD_LIBS([opencv_example opencv_wrapper],[${OPENCV_LIBS}])
          VLC_ADD_CFLAGS([opencv_wrapper],[${OPENCV_CFLAGS}])
          VLC_ADD_CXXFLAGS([opencv_example],[${OPENCV_CFLAGS}])
        ],
        [AC_MSG_ERROR([libopencv not found!])]
      )
    fi
fi


dnl
dnl  libsmbclient plugin
dnl
AC_ARG_ENABLE(smb,
  [  --enable-smb            smb input module (default enabled)])
if test "${enable_smb}" != "no"; then
  AC_CHECK_HEADERS(libsmbclient.h,
    [ VLC_ADD_PLUGIN([access_smb])
      VLC_ADD_LIBS([access_smb],[-lsmbclient]) ],
    [ if test -n "${enable_smb}"; then
        AC_MSG_ERROR([cannot find libsmbclient headers])
     fi ])
fi


dnl
dnl sftp access support
dnl
AC_ARG_ENABLE(sftp,
  [  --enable-sftp           sftp input access module (default disable)])
if test "${enable_sftp}" = "yes"; then
  AC_CHECK_HEADERS(libssh2.h, [
    VLC_ADD_PLUGIN([access_sftp])
    VLC_ADD_LIBS([access_sftp], [-lssh2])
  ])
fi

dnl
dnl  libdvbpsi ts demux/mux
dnl
AC_ARG_ENABLE(dvbpsi,
  [  --enable-dvbpsi         dvbpsi ts mux and demux module (default enabled)])
have_dvbpsi=no
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
        [ VLC_ADD_PLUGIN([ts])
          if test "${enable_sout}" != "no"; then
            VLC_ADD_PLUGIN([mux_ts])
          fi
          VLC_ADD_LIBS([mux_ts ts dvb],[-ldvbpsi])
	  have_dvbpsi=yes],
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
        VLC_ADD_PLUGIN([ts])
        if test "${enable_sout}" != "no"; then
          VLC_ADD_PLUGIN([mux_ts])
        fi
        VLC_ADD_CPPFLAGS([mux_ts ts dvb],[-I${real_dvbpsi_tree}/src])
        VLC_ADD_LIBS([mux_ts ts dvb],[${real_dvbpsi_tree}/src/.libs/libdvbpsi.a])
	have_dvbpsi=yes
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
      VLC_ADD_PLUGIN([ts])
      if test "${enable_sout}" != "no"; then
        VLC_ADD_PLUGIN([mux_ts])
      fi
      VLC_ADD_CPPFLAGS([mux_ts ts dvb],[${CPPFLAGS_test}])
      VLC_ADD_LIBS([mux_ts ts dvb],[${LDFLAGS_test} -ldvbpsi])
      have_dvbpsi=yes
    ],[
      if test -n "${enable_dvbpsi}"
      then
        AC_MSG_ERROR([Could not find libdvbpsi on your system: you may get it from www.videolan.org, you'll need at least version 0.1.1])
      fi
    ],
    [#if defined( HAVE_STDINT_H )
#   include <stdint.h>
#elif defined( HAVE_INTTYPES_H )
#   include <inttypes.h>
#endif
#include <dvbpsi/dvbpsi.h>
#include <dvbpsi/descriptor.h>
#include <dvbpsi/pat.h>
#include <dvbpsi/pmt.h>])
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
    VLC_ADD_PLUGIN([v4l])
  ],[])
  CPPFLAGS="${CPPFLAGS_save}"
fi

dnl
dnl libv4l1 support for video4linux.
dnl
AC_ARG_ENABLE( libv4l,
  [  --enable-libv4l        Libv4l Video4Linux support (default enabled)])
if test "${enable_libv4l}" != "no" -a "${enable_v4l}" != "no"
then
    PKG_CHECK_MODULES( LIBV4L, libv4l1, [
      VLC_ADD_LDFLAGS([v4l],[${LIBV4L_LIBS}])
      VLC_ADD_CFLAGS([v4l],[${LIBV4L_CFLAGS}])
      AC_DEFINE(HAVE_LIBV4L1, 1, Define if libv4l is available)],
      AC_MSG_WARN([LibV4L support disabled because libv4l development headers were not found])
    )
fi

dnl
dnl  Video4Linux2 plugin
dnl
AC_ARG_ENABLE(v4l2,
  [  --enable-v4l2           Video4Linux2 input support (default enabled)])
if test "${enable_v4l2}" != "no"
then
  AC_ARG_WITH(v4l2,
    [    --with-v4l2=PATH       path to a v4l2-enabled kernel tree],[],[])
  if test "${with_v4l2}" != "no" -a -n "${with_v4l2}"
  then
    VLC_ADD_CPPFLAGS([v4l2],[-I${with_v4l2}/include])
  fi

  CPPFLAGS="${CPPFLAGS_save} ${CPPFLAGS_v4l2}"
  AC_CHECK_HEADERS(linux/videodev2.h, [
    VLC_ADD_PLUGIN([v4l2])
  ],[])
  CPPFLAGS="${CPPFLAGS_save}"
fi

dnl
dnl libv4l2 support for video4linux.
dnl
AC_ARG_ENABLE( libv4l2,
  [  --enable-libv4l2        Libv4l2 Video4Linux2 support (default enabled)])
if test "${enable_libv4l2}" != "no" -a "${enable_v4l2}" != "no"
then
    PKG_CHECK_MODULES( LIBV4L2, libv4l2, [
      VLC_ADD_LDFLAGS([v4l2],[${LIBV4L2_LIBS}])
      VLC_ADD_CFLAGS([v4l2],[${LIBV4L2_CFLAGS}])
      AC_DEFINE(HAVE_LIBV4L2, 1, Define if libv4l2 is available)],
      AC_MSG_WARN([LibV4L2 support disabled because libv4l2 development headers were not found])
    )
fi

dnl
dnl  special access module for Hauppauge PVR cards
dnl
AC_ARG_ENABLE(pvr,
  [  --enable-pvr            PVR cards access module (default disabled)])
if test "${enable_pvr}" = "yes"
then
  VLC_ADD_PLUGIN([pvr])
  AC_ARG_WITH(videodev2,
    [  --with-videodev2=FILE   Location of videodev2.h file (default /usr/include/linux/videodev2.h)],[],[])
  if test "${with_videodev2}" != "no" -a -n "${with_videodev2}"
  then
    AC_DEFINE_UNQUOTED(VIDEODEV2_H_FILE, "${with_videodev2}", [Location of videodev2.h])
  fi

  AC_CACHE_CHECK([for new linux/videodev2.h],
      [ac_cv_new_linux_videodev2_h],
      [AC_TRY_COMPILE([
          #include <sys/types.h>
          #   ifdef VIDEODEV2_H_FILE
	  #   include VIDEODEV2_H_FILE
	  #   else
	  #   include <linux/videodev2.h>
	  #   endif
	  ],
          [struct v4l2_ext_controls ctrls; ctrls.ctrl_class = V4L2_CTRL_CLASS_MPEG; ],
          ac_cv_new_linux_videodev2_h=yes,
          ac_cv_new_linux_videodev2_h=no)])
  if test "${ac_cv_new_linux_videodev2_h}" != "no"; then
    AC_DEFINE(HAVE_NEW_LINUX_VIDEODEV2_H, 1, [Define if new linux/videodev2.h present])
  fi
fi

dnl
dnl  gnomeVFS access module
dnl
PKG_ENABLE_MODULES_VLC([GNOMEVFS], [access_gnomevfs], [gnome-vfs-2.0], [GnomeVFS access module], [auto])


dnl Need to test libcdio and libvcdinfo for a number of things. Do it now.
AC_ARG_ENABLE(libcdio,
  [  --enable-libcdio        CD input and control library support (default enabled)])

AC_ARG_ENABLE(vcdinfo,
  [  --enable-vcdinfo        VCD information library support (default enabled)])

have_libcdio=no
have_libvcdinfo=no
if test "${enable_libcdio}" != "no"
then
  PKG_CHECK_MODULES(LIBCDIO, libcdio >= 0.78.2,
     have_libcdio=yes
     AC_DEFINE(HAVE_LIBCDIO, 1,
     [Define if you have libcdio 0.78.2 or greater installed]),
     [AC_MSG_WARN(CD Reading and information library not found)])

  if test "${enable_vcdinfo}" != "no"
  then
    PKG_CHECK_MODULES(VCDINFO, libvcdinfo >= 0.7.22,
       [have_libvcdinfo=yes],
       [AC_MSG_WARN(VCD information library not found)])
  fi
fi

dnl
dnl  VCDX and CDDAX modules
dnl
AC_ARG_ENABLE(cddax,
  AS_HELP_STRING([--enable-cddax],[audio CD plugin with CD Text and CD paranoia
		  via libcdio (default disabled)]))

AC_ARG_ENABLE(libcddb,
  [  --enable-libcddb        CDDB support for libcdio audio CD (default enabled)])

if test "${enable_cddax}" = "yes"
then
  if test "$have_libcdio" = "yes"
  then
    AC_DEFINE(HAVE_CDDAX, 1, [Define for the audio CD plugin using libcdio])
    VLC_ADD_LIBS([cddax],[$LIBCDIO_LIBS])
    VLC_ADD_CFLAGS([cddax],[$LIBCDIO_CFLAGS])
    VLC_ADD_PLUGIN([cddax])
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
    VLC_ADD_PLUGIN([vcdx])
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
    VLC_ADD_PLUGIN([vcd])
    VLC_ADD_PLUGIN([cdda])
  ],[
    AC_MSG_RESULT(no)
  ])

  AC_MSG_CHECKING(for scsireq in sys/scsiio.h)
  AC_EGREP_HEADER(scsireq,sys/scsiio.h,[
    AC_MSG_RESULT(yes)
    VLC_ADD_PLUGIN([vcd])
    VLC_ADD_PLUGIN([cdda])
    AC_DEFINE(HAVE_SCSIREQ_IN_SYS_SCSIIO_H, 1, For NetBSD VCD support)
  ],[
    AC_MSG_RESULT(no)
  ])

  AC_MSG_CHECKING(for ioc_toc_header in sys/cdio.h)
  AC_EGREP_HEADER(ioc_toc_header ,sys/cdio.h,[
    AC_MSG_RESULT(yes)
    VLC_ADD_PLUGIN([vcd])
    VLC_ADD_PLUGIN([cdda])
    AC_DEFINE(HAVE_IOC_TOC_HEADER_IN_SYS_CDIO_H, 1, For FreeBSD VCD support)
  ],[
    AC_MSG_RESULT(no)
  ])

  if test "${SYS}" = "bsdi" -o "${SYS}" = "mingw32"
  then
    VLC_ADD_PLUGIN([vcd])
    VLC_ADD_PLUGIN([cdda])
  fi

  if test "${SYS}" = "darwin"
  then
    VLC_ADD_PLUGIN([vcd])
    VLC_ADD_PLUGIN([cdda])
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
  [  --enable-dvb            DVB-S/T/C card support (default enabled)])

if test "${enable_dvb}" != "no"
then
    AS_IF([test "${have_dvbpsi}" = "yes" ],[
    AC_ARG_WITH(dvb,
     [    --with-dvb=PATH       path to a dvb- and v4l2-enabled kernel tree],[],[])
     if test "${with_dvb}" != "no" -a -n "${with_dvb}"
     then
       VLC_ADD_CFLAGS([dvb],[-I${with_dvb}/include])
     fi
     CPPFLAGS="${CPPFLAGS_save} -I${with_dvb}/include"
     AC_CHECK_HEADERS(linux/dvb/version.h linux/dvb/frontend.h, [
     VLC_ADD_PLUGIN([dvb])
     ],[AC_MSG_WARN(linux-dvb headers not found, dvb disabled)])
     CPPFLAGS="${CPPFLAGS_save}"
   ],[
     AC_MSG_WARN([the dvb access module requires libdvbpsi])
    ])
fi

dnl
dnl  Screen capture module
dnl
AC_ARG_ENABLE(screen,
  [  --enable-screen         Screen capture support (default enabled)])
if test "${enable_screen}" != "no"; then
  if test "${SYS}" = "darwin"; then
    AC_CHECK_HEADERS(OpenGL/gl.h, [
      AC_CHECK_HEADERS(ApplicationServices/ApplicationServices.h, [
        VLC_ADD_PLUGIN([screen])
        VLC_ADD_LDFLAGS([screen],[-Wl,-framework,OpenGL,-framework,ApplicationServices])
      ])
    ])
  elif test "${SYS}" = "mingw32"; then
    VLC_ADD_PLUGIN([screen])
    VLC_ADD_LIBS([screen],[-lgdi32])
  elif test "${SYS}" = "mingwce"; then
    CPPFLAGS="${CPPFLAGS_save}"
  elif test "${SYS}" = "beos"; then
    VLC_ADD_PLUGIN([screen])
    VLC_ADD_CXXFLAGS([screen],[])
    VLC_ADD_LIBS([screen],[-lbe])
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
PKG_ENABLE_MODULES_VLC([OGG], [], [ogg >= 1.0], [Ogg demux support], [auto])
if test "${enable_sout}" != "no"; then
    PKG_ENABLE_MODULES_VLC([MUX_OGG], [], [ogg >= 1.0], [Ogg demux support], [auto])
fi

if test "${enable_sout}" != "no"; then
dnl Check for libshout
PKG_ENABLE_MODULES_VLC([SHOUT], [access_output_shout], [shout >= 2.1], [libshout output plugin], [auto])
fi

dnl
dnl  matroska demux plugin
dnl
AC_ARG_ENABLE(mkv,
  [  --enable-mkv            Matroska demux support (default enabled)])
if test "${enable_mkv}" != "no" -a "${CXX}" != ""; then
  AC_LANG_PUSH(C++)
  AC_CHECK_HEADERS(ebml/EbmlVersion.h, [
    AC_MSG_CHECKING(for libebml version >= 0.7.7)
    AC_EGREP_CPP(yes,
      [#include <ebml/EbmlVersion.h>
       #ifdef LIBEBML_VERSION
       #if LIBEBML_VERSION >= 0x000706
       yes
       #endif
       #endif],
      [AC_MSG_RESULT([yes])
        AC_CHECK_HEADERS(matroska/KaxVersion.h, [
          AC_MSG_CHECKING(for libmatroska version >= 0.8.0)
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
                VLC_ADD_PLUGIN([mkv])
                VLC_ADD_LIBS([mkv],[-lmatroska -lebml_pic])
              ],
                AC_CHECK_LIB(ebml, main, [
                  VLC_ADD_PLUGIN([mkv])
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
      VLC_ADD_PLUGIN([mod])
      VLC_ADD_LIBS([mod],[${real_mod_tree}/src/.libs/libmodplug.a -lstdc++])
      VLC_ADD_CXXFLAGS([mod],[-I${real_mod_tree}/include])
    else
      dnl  The given mod wasn't built
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([cannot find ${real_mod_tree}/src/.libs/libmodplug.a, make sure you compiled mod in ${with_mod_tree}])
    fi
  else
    AC_CHECK_HEADERS(libmodplug/modplug.h, [
      VLC_ADD_PLUGIN([mod])
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
  AC_CHECK_HEADERS([mpc/mpcdec.h], [
    VLC_ADD_PLUGIN([mpc])
    VLC_ADD_LIBS([mpc],[-lmpcdec])],
    [AC_CHECK_HEADERS([mpcdec/mpcdec.h], [
    VLC_ADD_PLUGIN([mpc])
    VLC_ADD_LIBS([mpc],[-lmpcdec])])])
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
      VLC_ADD_PLUGIN([gme])
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
dnl wmafixed plugin
dnl
AC_ARG_ENABLE(wma-fixed,
  [  --enable-wma-fixed      libwma-fixed module (default disabled)])
if test "${enable_wma_fixed}" = "yes"
then
  VLC_ADD_PLUGIN([wma_fixed])
fi

dnl
dnl shine fixed point mp3 encoder
dnl
AC_ARG_ENABLE(shine,
  [  --enable-shine          shine mp3 encoding module (default disabled)])
if test "${enable_shine}" = "yes"
then
  VLC_ADD_PLUGIN([shine])
fi

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
        VLC_ADD_PLUGIN([mpgatofixed32])
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
      VLC_ADD_PLUGIN([mpgatofixed32])
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
[  --disable-id3tag        id3tag metadata reader plugin (default disabled)])
AS_IF([test "${enable_id3tag}" = "yes"], [
  AC_CHECK_HEADERS(id3tag.h, [
      AS_IF([test "${have_zlib}" = "yes"],[
          VLC_ADD_LIBS([id3tag],[-lid3tag -lz])
          VLC_ADD_PLUGIN([id3tag])
       ])
    ])
])

AC_ARG_ENABLE(merge-ffmpeg,
[  --enable-merged-ffmpeg  merge FFmpeg-based plugins (default disabled)],, [
  enable_merge_ffmpeg="no"
])
AM_CONDITIONAL([MERGE_FFMPEG], [test "$enable_merge_ffmpeg" != "no"])

dnl
dnl  avcodec decoder/encoder plugin
dnl

if test "${with_ffmpeg_tree}"
then
  AC_MSG_WARN([--with-ffmpeg-tree is deprecated. Use PKG_CONFIG_PATH instead.])
fi

AC_ARG_ENABLE(avcodec,
[  --enable-avcodec        libavcodec codec (default enabled)])
if test "${enable_avcodec}" != "no"
then
  PKG_CHECK_MODULES(AVCODEC,[libavcodec >= 51.48.0 libavutil],
    [
      VLC_SAVE_FLAGS
      CPPFLAGS="${CPPFLAGS} ${AVCODEC_CFLAGS}"
      CFLAGS="${CFLAGS} ${AVCODEC_CFLAGS}"
      AC_CHECK_HEADERS(libavcodec/avcodec.h ffmpeg/avcodec.h)
      AC_CHECK_HEADERS(libavutil/avutil.h ffmpeg/avutil.h)
      VLC_ADD_PLUGIN([avcodec])
      VLC_ADD_LIBS([avcodec],[$AVCODEC_LIBS])
      VLC_ADD_CFLAGS([avcodec],[$AVCODEC_CFLAGS])
      VLC_RESTORE_FLAGS
      have_avcodec="yes"
    ],[
      AC_MSG_ERROR([Could not find libavcodec or libavutil. Use --disable-avcodec to ignore this error.])
  ])
fi

dnl
dnl libva needs avcodec
dnl
AC_ARG_ENABLE(libva,
  [  --enable-libva         libva VAAPI support (default disabled)])
AS_IF([test "${enable_libva}" = "yes"], [
  AS_IF([test "x${have_avcodec}" = "xyes"], [
    PKG_CHECK_MODULES(LIBVA, [libva libva-x11],
      [
        VLC_ADD_LIBS([avcodec],[$LIBVA_LIBS])
        VLC_ADD_CFLAGS([avcodec],[$LIBVA_CFLAGS])
        AC_DEFINE(HAVE_AVCODEC_VAAPI, 1, [Define if avcodec has to be built with VAAPI support.])
      ],[
        AC_MSG_ERROR([Could not find libva.])
    ])
  ],[
    AC_MSG_ERROR([libva VAAPI support depends on libavcodec.])
  ])
])

dnl
dnl stream_out switcher needs libavcodec
dnl
AC_ARG_ENABLE(switcher,
  [  --enable-switcher       Stream-out switcher plugin (default disabled)])
AS_IF([test "${enable_switcher}" = "yes"], [
  AS_IF([test "x${have_avcodec}" = "xyes"], [
    VLC_ADD_PLUGIN([stream_out_switcher])
    VLC_ADD_LIBS([stream_out_switcher],[$AVCODEC_LIBS $AVUTIL_LIBS])
    VLC_ADD_CFLAGS([stream_out_switcher],[$AVCODEC_CFLAGS $AVUTIL_CFLAGS])
  ],[AC_MSG_ERROR([Stream_out switcher depends on avcodec])
  ])
])


dnl
dnl  avformat demuxer/muxer plugin
dnl

AC_ARG_ENABLE(avformat,
[  --enable-avformat       libavformat containers (default enabled)])
if test "${enable_avformat}" != "no"
then
  PKG_CHECK_MODULES(AVFORMAT,[libavformat libavutil],
    [
      VLC_SAVE_FLAGS
      CPPFLAGS="${CPPFLAGS} ${AVFORMAT_CFLAGS}"
      CFLAGS="${CFLAGS} ${AVFORMAT_CFLAGS}"
      AC_CHECK_HEADERS(libavformat/avformat.h ffmpeg/avformat.h)
      AC_CHECK_HEADERS(libavutil/avutil.h ffmpeg/avutil.h)
      AS_IF([test "$enable_merge_ffmpeg" = "no"], [
        VLC_ADD_PLUGIN([avformat])
        VLC_ADD_LIBS([avformat],[$AVFORMAT_LIBS $AVUTIL_LIBS])
        VLC_ADD_CFLAGS([avformat],[$AVFORMAT_CFLAGS $AVUTIL_CFLAGS])
      ], [
        VLC_ADD_LIBS([avcodec],[$AVFORMAT_LIBS $AVUTIL_LIBS])
        VLC_ADD_CFLAGS([avcodec],[$AVFORMAT_CFLAGS $AVUTIL_CFLAGS])
      ])
      VLC_RESTORE_FLAGS
    ],[
      AC_MSG_ERROR([Could not find libavformat or libavutil. Use --disable-avformat to ignore this error.])
  ])
fi

dnl
dnl  swscale image scaling and conversion plugin
dnl

AC_ARG_ENABLE(swscale,
  AS_HELP_STRING([--enable-swscale],[libswscale image scaling and conversion
		  (default enabled)]))
if test "${enable_swscale}" != "no"
then
  PKG_CHECK_MODULES(SWSCALE,[libswscale],
    [
      VLC_SAVE_FLAGS
      CPPFLAGS="${CPPFLAGS} ${SWSCALE_CFLAGS}"
      CFLAGS="${CFLAGS} ${SWSCALE_CFLAGS}"
      AC_CHECK_HEADERS(libswscale/swscale.h ffmpeg/swscale.h)
      VLC_ADD_PLUGIN([swscale])
      VLC_ADD_LIBS([swscale],[$SWSCALE_LIBS])
      VLC_ADD_CFLAGS([swscale],[$SWSCALE_CFLAGS])
      VLC_RESTORE_FLAGS
    ],[
      AC_MSG_ERROR([Could not find libswscale. Use --disable-swscale to ignore this error. Proper software scaling and some video chroma conversion will be missing.])
  ])
fi

dnl
dnl  postproc plugin
dnl

AC_ARG_ENABLE(postproc,
[  --enable-postproc       libpostproc image post-processing (default enabled)])
if test "${enable_postproc}" != "no"
then
  PKG_CHECK_MODULES(POSTPROC,[libpostproc libavutil],
    [
      VLC_SAVE_FLAGS
      CPPFLAGS="${CPPFLAGS} ${POSTPROC_CFLAGS}"
      CFLAGS="${CFLAGS} ${POSTPROC_CFLAGS}"
      AC_CHECK_HEADERS(postproc/postprocess.h)
      VLC_ADD_PLUGIN([postproc])
      VLC_ADD_LIBS([postproc],[$POSTPROC_LIBS $AVUTIL_LIBS])
      VLC_ADD_CFLAGS([postproc],[$POSTPROC_CFLAGS $AVUTIL_CFLAGS])
      VLC_RESTORE_FLAGS
    ],[
      AC_MSG_ERROR([Could not find libpostproc. Use --disable-postproc to ignore this error.])
  ])
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
      VLC_ADD_PLUGIN([faad])
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
      VLC_ADD_PLUGIN([faad])
      VLC_ADD_LIBS([faad],[-lfaad]) ],
      AC_CHECK_LIB(faad, NeAACDecOpen, [
        VLC_ADD_PLUGIN([faad])
        VLC_ADD_LIBS([faad],[-lfaad]) ],
        [ AC_MSG_ERROR([Cannot find libfaad library...]) ]))
    LDFLAGS="${LDFLAGS_save}"
    CPPFLAGS="${CPPFLAGS_save}"
  fi
fi

dnl
dnl twolame encoder plugin
dnl
PKG_ENABLE_MODULES_VLC([TWOLAME], [], [twolame], [MPEG Audio Layer 2 encoder], [auto], [], [], [ -DLIBTWOLAME_STATIC])

dnl
dnl  QuickTime plugin
dnl
AC_ARG_ENABLE(quicktime,
  [  --enable-quicktime      QuickTime module (deprecated)])
if test "${enable_quicktime}" = "yes"; then
  if test "${SYS}" = "mingw32"; then
    VLC_ADD_PLUGIN([quicktime])
  else
  AC_CHECK_HEADERS(QuickTime/QuickTime.h,
    [ VLC_ADD_PLUGIN([quicktime])
      VLC_ADD_LDFLAGS([quicktime],[-Wl,-framework,QuickTime,-framework,Carbon])
    ], [ AC_MSG_ERROR([cannot find QuickTime headers]) ])
  fi
fi

dnl
dnl  Real plugin
dnl
AC_ARG_ENABLE(real,
  [  --enable-real           Real media module (default disabled)])
if test "${enable_real}" = "yes"; then
  VLC_ADD_PLUGIN([realaudio])
  VLC_ADD_PLUGIN([realvideo])
fi

dnl
dnl  Real RTSP plugin
dnl
AC_ARG_ENABLE(realrtsp,
  [  --enable-realrtsp       Real RTSP module (default disabled)])
if test "${enable_realrtsp}" = "yes"; then
  VLC_ADD_PLUGIN([access_realrtsp])
fi

AC_ARG_ENABLE(libsysfs,
  [  --enable-libsysfs       Get user key from firewire connected iPod via libsysfs (default enabled)])

AS_IF([test "${enable_libsysfs}" != "no"],[
  AC_CHECK_HEADERS(sysfs/libsysfs.h, [
    VLC_ADD_LIBS([mp4 mkv],[-lsysfs])
  ] )
])

dnl
dnl skins2 module
dnl
AC_ARG_ENABLE(libtar,
  [  --enable-libtar         libtar support for skins2 (default enabled)])

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
        VLC_ADD_PLUGIN([a52tofloat32])
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
        VLC_ADD_PLUGIN([a52tofloat32])
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
PKG_ENABLE_MODULES_VLC([DCA], [dtstofloat32], [libdca >= 0.0.5], [DTS Coherent Acoustics support with libdca], [auto])

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
AC_ARG_ENABLE(tremor,
  [  --enable-tremor         Tremor decoder support (default disabled)])
if test "${enable_tremor}" = "yes"
then
  AC_CHECK_HEADERS(tremor/ivorbiscodec.h, [
    VLC_ADD_PLUGIN([tremor])
    VLC_ADD_LIBS([tremor],[-lvorbisidec -logg])
   ],[])
fi

dnl
dnl  Speex plugin
dnl
PKG_ENABLE_MODULES_VLC([SPEEX], [], [ogg speex >= 1.0.5], [Speex decoder support], [auto])

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
      VLC_ADD_PLUGIN([tarkin])
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
PKG_ENABLE_MODULES_VLC([THEORA], [], [ogg theora >= 1.0], [experimental theora codec], [auto])

dnl
dnl  dirac encoder plugin
dnl
PKG_ENABLE_MODULES_VLC([DIRAC], [], [dirac >= 0.10.0], [dirac encoder], [auto])

dnl
dnl  schroedinger decoder plugin (for dirac format video)
dnl
PKG_ENABLE_MODULES_VLC([SCHROEDINGER], [], [schroedinger-1.0 >= 1.0.6], [dirac decoder using schroedinger], [auto])

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
    VLC_ADD_PLUGIN([png])
    VLC_ADD_PLUGIN([osdmenu])
    VLC_ADD_PLUGIN([osd_parser])],
    [],[-lz])
    LDFLAGS="${LDFLAGS_save}"
  ])
fi
AM_CONDITIONAL(BUILD_OSDMENU, [test "${enable_png}" != "no"])

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
        VLC_ADD_PLUGIN([x264])
        VLC_ADD_LDFLAGS([x264],[${X264_LIBS}])
        VLC_ADD_CFLAGS([x264],[${X264_CFLAGS}])
        if echo ${X264_LIBS} |grep -q 'pthreadGC2'; then
          VLC_ADD_CFLAGS([x264], [-DPTW32_STATIC_LIB])
        fi
      ],[
        AC_MSG_ERROR([the specified tree hasn't been compiled])
      ])
      LDFLAGS="${LDFLAGS_save}"
    else
      AC_MSG_RESULT(no)
      AC_MSG_ERROR([the specified tree doesn't have x264.h])
    fi
  else
      PKG_CHECK_MODULES(X264,x264 >= 0.76, [
        VLC_ADD_PLUGIN([x264])
        VLC_ADD_LDFLAGS([x264],[${X264_LIBS}])

        AC_CACHE_CHECK([if linker supports -Bsymbolic],
          [ac_cv_ld_bsymbolic],
          [LDFLAGS="${LDFLAGS_vlc} -Wl,-Bsymbolic"
            AC_TRY_LINK([],,ac_cv_ld_bsymbolic=yes,ac_cv_ld_bsymbolic=no)
            LDFLAGS="${LDFLAGS_save}"
          ])
        AS_IF([test "${ac_cv_ld_bsymbolic}" != "no"], [
          VLC_ADD_LDFLAGS([x264],[-Wl,-Bsymbolic])
        ])

        VLC_ADD_CFLAGS([x264],[${X264_CFLAGS}])
        if echo ${X264_LIBS} |grep -q 'pthreadGC2'; then
          VLC_ADD_CFLAGS([x264], [-DPTW32_STATIC_LIB])
        fi
      ],[
        if test "${enable_x264}" = "yes"; then
            AC_MSG_ERROR([Could not find libx264 >= 0.76 on your system: you may get it from http://www.videolan.org/x264.html])
          fi
      ])
    LDFLAGS="${LDFLAGS_save}"
  fi
fi

dnl
dnl libfluidsynth (MIDI synthetizer) plugin
dnl
PKG_ENABLE_MODULES_VLC([FLUIDSYNTH], [], [fluidsynth], [MIDI synthetiser with libfluidsynth], [auto])

dnl
dnl Teletext Modules
dnl vbi decoder plugin (using libzbvi)
dnl telx module
dnl uncompatible
dnl
AC_ARG_ENABLE(zvbi,
  AS_HELP_STRING([--enable-zvbi],[VBI (inc. Teletext) decoding support with
                  libzvbi (default enabled)]))
AC_ARG_ENABLE(telx,
  AS_HELP_STRING([--enable-telx],[Teletext decoding module (conflicting with
                  zvbi) (default enabled if zvbi is absent)]))

AS_IF( [test "${enable_zvbi}" != "no"],[
    PKG_CHECK_MODULES(ZVBI,
        zvbi-0.2 >= 0.2.25,
        [
          VLC_ADD_LIBS([zvbi],[$ZVBI_LIBS])
          if test "${SYS}" = "mingw32"; then
              VLC_ADD_LIBS([zvbi],[ -lpthreadGC2])
          fi
          VLC_ADD_CFLAGS([zvbi],[$ZVBI_CFLAGS])
          VLC_ADD_PLUGIN([zvbi])
          AC_DEFINE(ZVBI_COMPILED, 1, [Define if the zvbi module is built])
          AS_IF( [test "${enable_telx}" = "yes"],[
                  AC_MSG_WARN([The zvbi and telx modules are uncompatibles.
                               Using zvbi.])
                  ])
        ],[
          AC_MSG_WARN(ZVBI library not found. Enabling the telx module instead)
        ])
    ])
AS_IF( [test "${enable_telx}" != "no" ],[
  VLC_ADD_PLUGIN([telx])
  ])

dnl
dnl libass subtitle rendering module
dnl
AC_ARG_ENABLE(libass,
  [  --enable-libass         Subtitle support using libass (default enabled)])
AS_IF( [test "${enable_libass}" != "no"], [
  PKG_CHECK_MODULES(LIBASS, libass >= 0.9.6,
      [
        VLC_ADD_LIBS([libass],[$LIBASS_LIBS])
        VLC_ADD_CFLAGS([libass],[$LIBASS_CFLAGS])
        VLC_ADD_PLUGIN([libass])

        AC_CHECK_HEADERS(fontconfig/fontconfig.h,
          [VLC_ADD_CPPFLAGS([libass],[-DHAVE_FONTCONFIG])
           VLC_ADD_LIBS([libass],[-lfontconfig])
       ])
      ],[
        AC_MSG_WARN([LIBASS library not found])
      ])
  ])

dnl
dnl asa demuxer
dnl
AC_ARG_ENABLE(asademux,
  [  --enable-asademux       asa subtitle demuxing (default disabled)])
AS_IF( [test "${enable_asademux}" = "yes"], [
  PKG_CHECK_MODULES(PCRE,
      libpcre >= 6.5,
      [
        VLC_ADD_LDFLAGS([asademux],[$PCRE_LIBS])
        VLC_ADD_CFLAGS([asademux],[$PCRE_CFLAGS])
        if test "${SYS}" = "mingw32"; then
            VLC_ADD_CPPFLAGS([asademux],[-DPCRE_STATIC])
        fi
        VLC_ADD_PLUGIN([asademux])
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
  VLC_ADD_PLUGIN([cmml])
fi

dnl
dnl  kate decoder plugin
dnl
AC_ARG_ENABLE(kate,
[  --enable-kate           kate codec (default enabled)])
AS_IF([test "${enable_kate}" != "no"], [
  PKG_CHECK_MODULES(KATE,[kate >= 0.1.5], [
      VLC_ADD_PLUGIN([kate])
      VLC_ADD_CFLAGS([kate],[$KATE_CFLAGS])
      VLC_ADD_LIBS([kate],[$KATE_LIBS]) ],[
        AC_CHECK_HEADERS(kate/kate.h, [
          AC_CHECK_LIB(kate, kate_decode_init, [
            VLC_ADD_PLUGIN([kate])
            kate_libs="-lkate -logg"
            VLC_ADD_LDFLAGS([kate],[${kate_libs}]) ],[
            AS_IF([test "x${enable_kate}" != "x"], [
              AC_MSG_ERROR([libkate doesn't appear to be installed on your system.
              You also need to check that you have a libogg posterior to the 1.0 release.])
            ])
	  ], [-lkate -logg])
        ],[
          AS_IF([test "x${enable_kate}" != "x"], [
            AC_MSG_ERROR([libkate headers do not appear to be installed on your system.
            You also need to check that you have a libogg posterior to the 1.0 release.])
          ])
        ])
  ])
])


dnl
dnl  tiger rendering for kate decoder plugin
dnl
AC_ARG_ENABLE(tiger,
[  --enable-tiger          Tiger rendering library for Kate streams (default enabled)])
AS_IF([test "${enable_tiger}" != "no"], [
  PKG_CHECK_MODULES(TIGER,[tiger >= 0.3.1], [
      AC_DEFINE(HAVE_TIGER, 1, [Define if libtiger is available.])
      VLC_ADD_CFLAGS([kate],[$TIGER_CFLAGS])
      VLC_ADD_LIBS([kate],[$TIGER_LIBS]) ],[
        AS_IF([test "x${enable_tiger}" != "x"], [
          AC_MSG_ERROR([libtiger does not appear to be installed on your system.])
        ])
  ])
])



dnl
dnl  Video plugins
dnl

AC_ARG_WITH(,[Video plugins:])

dnl
dnl  Xlib
dnl

AC_PATH_XTRA()
AC_ARG_ENABLE(x11,
  [  --enable-x11            X11 support with Xlib (default enabled)],, [
  AS_IF([test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"], [
    enable_x11="yes"
  ], [
    enable_x11="no"
  ])
])

CPPFLAGS="${CPPFLAGS_save} ${X_CFLAGS}"

AS_IF([test "${enable_x11}" != "no"], [
  AC_CHECK_HEADERS(X11/Xlib.h)

dnl Check for DPMS
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
])

dnl
dnl  GLX module
dnl  (enabled by default except on win32)
dnl
AC_ARG_ENABLE(glx,
  [  --enable-glx            X11 OpenGL (GLX) support (default enabled)],, [
  enable_glx="$enable_x11"
])
AS_IF([test "${enable_glx}" != "no"], [
  AC_CHECK_HEADERS(X11/Xlib.h GL/glu.h GL/glx.h)
  AC_COMPILE_IFELSE(AC_LANG_PROGRAM(
    [[#if !defined(HAVE_X11_XLIB_H) || !defined(HAVE_GL_GLU_H) || !defined(HAVE_GL_GLX_H)
    choke me
    #endif]]),
    [
      VLC_ADD_PLUGIN([glx])
      VLC_ADD_LIBS([glx],[${X_LIBS} ${X_PRE_LIBS} -lX11 -lXext -lGL -lGLU])
      VLC_ADD_CPPFLAGS([glx],[${X_CFLAGS}])
    ],[AC_MSG_ERROR([Please install GL development package. Alternatively you can also configure with --disable-glx.])])
])

dnl
dnl  XVMC module
dnl  (disabled by default except on win32)
dnl
AC_ARG_ENABLE(xvmc,
  [  --enable-xvmc           XvMC support (default disabled)],, [
  enable_xvmc="no"
])
AS_IF([test "${enable_xvmc}" != "no"], [
  AC_CHECK_HEADERS(X11/extensions/vldXvMC.h, [
    AS_IF([test "${enable_libmpeg2}" = "yes"], [
      VLC_ADD_PLUGIN([xvmc])
      VLC_ADD_LIBS([xvmc],[${X_LIBS} ${X_PRE_LIBS} -lX11 -lXext -lXvMCW -lXv ${LIBMPEG2_LIBS}])
      VLC_ADD_CPPFLAGS([xvmc],[${X_CFLAGS} ${LIBMPEG2_CFLAGS}])
    ], [
      AC_MSG_ERROR([XVideo-MotionCompensation needs libmpeg2 which isn't available])
    ])
  ], [
    AC_MSG_ERROR([XVideo-MotionCompensation extension not found!])
  ])
])

dnl End of Xlib tests
CPPFLAGS="${CPPFLAGS_save}"


dnl
dnl  X C Bindings modules
dnl
AC_ARG_ENABLE(xcb,
  [  --enable-xcb            X11 support with XCB (default enabled)],, [
  AS_IF([test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"], [
    enable_xcb="yes"
  ], [
    enable_xcb="no"
  ])
])
AC_ARG_ENABLE(xvideo,
  [  --enable-xvideo         XVideo support (default enabled)],, [
    enable_xvideo="$enable_xcb"
])

need_xid_provider="no"
AS_IF([test "${enable_xcb}" != "no"], [
  dnl libxcb
  PKG_CHECK_MODULES(XCB, [xcb])
  PKG_CHECK_MODULES(XCB_SHM, [xcb-shm])
  VLC_ADD_PLUGIN([xcb_x11])
  VLC_ADD_PLUGIN([xcb_screen])
  VLC_ADD_PLUGIN([xcb_apps])
  VLC_SET_CFLAGS_WERROR([xcb_screen], [-Wno-error=uninitialized]) # some gcc report a warning which doesn't reveal an error

  AS_IF([test "${enable_xvideo}" != "no"], [
    PKG_CHECK_MODULES(XCB_XV, [xcb-xv >= 1.1.90.1], [
      VLC_ADD_PLUGIN([xcb_xv])
    ], [
      PKG_CHECK_MODULES(XCB_XV, [xcb-xv], [
        VLC_ADD_PLUGIN([xcb_xv])
        VLC_ADD_CFLAGS([xcb_xv], [-DXCB_XV_OLD])
      ])
    ])
  ])

  PKG_CHECK_MODULES(XCB_RANDR, [xcb-randr >= 1.3], [
    VLC_ADD_PLUGIN([panoramix])
    VLC_ADD_LIBS([panoramix],[${XCB_RANDR_LIBS} ${XCB_LIBS}])
    VLC_ADD_CFLAGS([panoramix],[${XCB_RANDR_CFLAGS} ${XCB_CFLAGS}])
  ], [true])

  dnl xcb-utils
  PKG_CHECK_MODULES(XCB_KEYSYMS, [xcb-keysyms >= 0.3.4], [
    have_xcb_keysyms="yes"
  ], [
    PKG_CHECK_MODULES(XCB_KEYSYMS, [xcb-keysyms], [
      have_xcb_keysyms="yes"
      VLC_ADD_CFLAGS([globalhotkeys], [-DXCB_KEYSYM_OLD_API])
    ], [
      have_xcb_keysyms="no"
      need_xid_provider="yes"
    ])
  ])

  AS_IF([test "${have_xcb_keysyms}" = "yes"], [
    PKG_CHECK_MODULES(XPROTO, [xproto])
    VLC_ADD_PLUGIN([xcb_window])
    VLC_ADD_PLUGIN([globalhotkeys])
    VLC_ADD_CFLAGS([globalhotkeys], [${XCB_KEYSYMS_CFLAGS} ${XCB_CFLAGS}])
    VLC_ADD_LIBS([globalhotkeys], [${XCB_KEYSYMS_LIBS} ${XCB_LIBS}])
  ])
  VLC_ADD_PLUGIN([xdg_screensaver])
])


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
      VLC_ADD_PLUGIN([opengl])
      if test "${SYS}" != "mingw32"; then
        VLC_ADD_LIBS([opengl],[${X_LIBS} -lGL -lGLU])
      else
        VLC_ADD_LIBS([opengl],[-lopengl32 -lglu32])
      fi
    ])
  else
    dnl OS X special case (no GL/gl.h but OpenGL/gl.h)
    VLC_ADD_PLUGIN([opengl])
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
  AC_PATH_PROG(SDL_CONFIG, sdl-config, no, ${SDL_PATH})
  SDL_HEADER="SDL/SDL.h"
  SDL_IMAGE="SDL/SDL_image.h"

  if test "${SDL_CONFIG}" != "no";  then
     PKG_CHECK_MODULES(SDL, [sdl >= 1.2.10], [
        # SDL on Darwin is heavily patched and can only run SDL_image
        if test "${SYS}" != "darwin"; then
          VLC_ADD_PLUGIN([vout_sdl])
          VLC_ADD_PLUGIN([aout_sdl])
        fi
        VLC_ADD_CFLAGS([vout_sdl aout_sdl sdl_image],[`${SDL_CONFIG} --cflags | sed 's,SDL,,'`])
        VLC_ADD_LIBS([vout_sdl aout_sdl sdl_image],[`${SDL_CONFIG} --libs | sed 's,-rdynamic,,'`])

        # SDL_image
        AS_IF([ test "${enable_sdl_image}" != "no"],[
          AC_CHECK_HEADERS("SDL/SDL_image.h", [
            VLC_ADD_PLUGIN([sdl_image])
            AC_CHECK_LIB(png, png_set_rows, [VLC_ADD_LIBS([sdl_image],[-lpng -lz])],[],[-lz])
            AC_CHECK_LIB(jpeg, jpeg_start_decompress, [VLC_ADD_LIBS([sdl_image],[-ljpeg])])
            AC_CHECK_LIB(tiff, TIFFClientOpen, [VLC_ADD_LIBS([sdl_image],[-ltiff])])
            VLC_ADD_LIBS([sdl_image], [-lSDL_image])],
            [ AC_MSG_WARN([The development package for SDL_image is not installed.
      You should install it alongside your SDL package.])
            ])
        ])
     ],[
       AC_MSG_ERROR([The development package for SDL is too old. You need 1.2.10.
                    Alternatively you can also configure with --disable-sdl.])
     ])
  else if test "${enable_sdl}" =  "yes"; then
    AC_MSG_ERROR([I could not find the SDL package. You can download libSDL
from http://www.libsdl.org/, or configure with --disable-sdl. Have a nice day.
    ])
      fi
  fi
fi

dnl
dnl  freetype module
dnl
AC_ARG_ENABLE(freetype,
  [  --enable-freetype       freetype support (default enabled)])
AC_ARG_ENABLE(fribidi,
  [  --enable-fribidi        fribidi support (default enabled)])
AC_ARG_ENABLE(fontconfig,
  [  --enable-fontconfig,    fontconfig support (default enabled)])
if test "${enable_freetype}" != "no"
then
   PKG_CHECK_MODULES(FREETYPE, freetype2,[
      VLC_ADD_PLUGIN([freetype])
      have_freetype=yes
      VLC_ADD_CPPFLAGS([freetype skins2],[${FREETYPE_CFLAGS}])
      if test "${SYS}" = "mingw32"; then
        VLC_ADD_LIBS([freetype],[-liconv -lz])
      fi
      VLC_ADD_LIBS([freetype skins2],[${FREETYPE_LIBS}])
      if test "${enable_fontconfig}" != "no"
      then
        AC_CHECK_HEADERS(fontconfig/fontconfig.h,
          [VLC_ADD_CPPFLAGS([freetype],[-DHAVE_FONTCONFIG])
           VLC_ADD_LIBS([freetype],[-lfontconfig])])
        AC_CHECK_HEADERS(Carbon/Carbon.h,
          [VLC_ADD_LDFLAGS([freetype],[-Wl,-framework,Carbon])])
      fi

     dnl fribidi support
      if test "${enable_fribidi}" != "no"
      then
        PKG_CHECK_MODULES(FRIBIDI, fribidi, [
          VLC_ADD_CPPFLAGS([freetype skins2], [${FRIBIDI_CFLAGS} -DHAVE_FRIBIDI])
          VLC_ADD_LIBS([freetype skins2], [${FRIBIDI_LIBS}])
        ])
      fi

  ],[
  have_freetype=no
  AS_IF([ test "${enable_freetype}" =  "yes"],[
    AC_MSG_ERROR([I couldn't find the freetype package. You can download libfreetype2
from http://www.freetype.org/, or configure with --disable-freetype. Have a nice day.
      ])
    ])
  ])

fi

dnl
dnl  libxml2 module
dnl
PKG_ENABLE_MODULES_VLC([LIBXML2], [xml], [libxml-2.0 >= 2.5], [libxml2 support],[auto])

dnl
dnl  SVG module
dnl
PKG_ENABLE_MODULES_VLC([SVG], [], [librsvg-2.0 >= 2.9.0], [SVG rendering library],[auto])

dnl
dnl Snapshot vout module (with cache)
dnl
AC_ARG_ENABLE(snapshot,
  [  --enable-snapshot       snapshot module (default disabled)])
if test "${enable_snapshot}" = "yes"
then
  VLC_ADD_PLUGIN([snapshot])
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
  VLC_ADD_PLUGIN([qte])
  NEED_QTE_MAIN=yes
  CPPFLAGS="${CPPFLAGS_save} ${CXXFLAGS_qte}"
  AC_CHECK_HEADERS(qt.h jpeglib.h, ,[
    AC_MSG_ERROR([echo "Cannot find QT Embedded development headers."])
  ] )
  CPPFLAGS="${CPPFLAGS_save}"
fi

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
    VLC_ADD_PLUGIN([hd1000v])
    VLC_ADD_LIBS([hd1000v],[-lCascade -ldvbpsi -lmad])
  fi
  AC_LANG_POP([C++])
fi

dnl
dnl  Windows DirectX module
dnl

if test "${SYS}" = "mingw32" -o "${SYS}" = "mingwce"
then
  VLC_ADD_PLUGIN([panoramix])
fi

AC_ARG_ENABLE(directx,
  [  --enable-directx        Win32 DirectX support (default enabled on Win32)])
if test "${enable_directx}" != "no"
then
  if test "${SYS}" = "mingw32" -o "${SYS}" = "mingwce"
  then
      AC_CHECK_HEADERS(ddraw.h,
      [ VLC_ADD_PLUGIN([directx])
        VLC_ADD_PLUGIN([aout_directx])
        VLC_ADD_LIBS([directx],[-lgdi32])
      ],[AC_MSG_ERROR([Cannot find DirectX headers!])]
      )
      AC_CHECK_HEADERS(GL/gl.h,
      [ VLC_ADD_PLUGIN([glwin32])
        VLC_ADD_LIBS([glwin32],[-lopengl32 -lgdi32])
      ])
      AC_CHECK_HEADERS(d3d9.h,
      [ VLC_ADD_PLUGIN([direct3d])
        VLC_ADD_LIBS([direct3d],[-lgdi32])
      ])
      VLC_ADD_LIBS([direct3d directx glwin32],[-lole32])
  fi
fi

dnl
dnl  win32 GDI plugin
dnl
AC_ARG_ENABLE(wingdi,
  [  --enable-wingdi         Win32 GDI module (default enabled on Win32)])
if test "${enable_wingdi}" != "no"; then
  if test "${SYS}" = "mingw32"; then
    VLC_ADD_PLUGIN([wingdi])
    VLC_ADD_LIBS([wingdi],[-lgdi32 -lole32])
  fi
  if test "${SYS}" = "mingwce"; then
    VLC_ADD_PLUGIN([wingdi])
    VLC_ADD_PLUGIN([wingapi])
    VLC_ADD_LIBS([wingdi],[-laygshell])
    VLC_ADD_LIBS([wingapi],[-laygshell])
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
        VLC_ADD_PLUGIN([fb])
      ])
    fi

dnl
dnl  Linux MGA module
dnl
AC_ARG_ENABLE(mga,
  [  --enable-mga            Linux kernel Matrox support (default disabled)],
  [ if test "${enable_mga}" = "yes"
    then
      VLC_ADD_PLUGIN([mga])
    fi ])

dnl
dnl  OMAP Framebuffer module
dnl
AC_ARG_ENABLE(omapfb,
  [  --enable-omapfb         OMAP framebuffer support (default disabled)])
    if test "${enable_omapfb}" = "yes"
    then
      AC_CHECK_HEADERS(asm/arch-omap/omapfb.h, [
        VLC_ADD_PLUGIN([omapfb])
        AC_CHECK_HEADERS(X11/Xlib.h, [
          VLC_ADD_LIBS([omapfb],[${X_LIBS} ${X_PRE_LIBS} -lX11]) ])
      ])
      VLC_ADD_PLUGIN([swscale_omap])
    fi

dnl
dnl  SVGAlib module
dnl
AC_ARG_ENABLE(svgalib,
  [  --enable-svgalib        SVGAlib support (default disabled)])
if test "${enable_svgalib}" = "yes"
then
  VLC_ADD_PLUGIN([svgalib])
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
        VLC_ADD_PLUGIN([directfb])
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
  AC_CHECK_HEADER([ggi/ggi.h],[
    VLC_ADD_PLUGIN([ggi])
    VLC_ADD_LIBS([ggi],[-lggi])
    AC_ARG_WITH(ggi,
      [    --with-ggi=PATH       path to libggi],
      [ if test "${with_ggi}" != "no" -a -n "${with_ggi}"
        then
          VLC_ADD_CPPFLAGS([ggi],[-I${with_ggi}/include])
          VLC_ADD_LIBS([ggi],[-L${with_ggi}/lib])
        fi ])
    ],[
       AC_MSG_ERROR([LibGGI development files can't be found])
    ])
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
    VLC_ADD_PLUGIN([aa])
    VLC_ADD_LIBS([aa],[-laa])
  fi
fi

dnl
dnl  libcaca plugin
dnl
PKG_ENABLE_MODULES_VLC([CACA], [], [caca >= 0.99.beta1], [libcaca output],[auto])

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
  AC_CHECK_HEADERS([soundcard.h sys/soundcard.h], [
    VLC_ADD_PLUGIN([oss])
    VLC_ADD_PLUGIN([access_oss])
    AC_CHECK_LIB(ossaudio,main,[VLC_ADD_LIBS([oss],[-lossaudio])
                                VLC_ADD_LIBS([access_oss],[-lossaudio])])
  ])
fi

dnl
dnl  Pulseaudio module
dnl
PKG_ENABLE_MODULES_VLC([PULSE], [], [libpulse >= 0.9.11], [Pulseaudio support], [auto])

dnl
dnl  Portaudio module
dnl
PKG_ENABLE_MODULES_VLC([PORTAUDIO], [], [portaudio-2.0], [Portaudio library support], [auto])

dnl
dnl  ALSA module
dnl
PKG_ENABLE_MODULES_VLC([ALSA], [], [alsa >= 1.0.0], [Alsa sound support for linux], [auto])
PKG_ENABLE_MODULES_VLC([ACCESS_ALSA], [access_alsa], [alsa >= 1.0.0], [Alsa access for linux], [auto])

dnl
dnl  win32 waveOut plugin
dnl
AC_ARG_ENABLE(waveout,
  [  --enable-waveout        Win32 waveOut module (default enabled on Win32)])
if test "${enable_waveout}" != "no"; then
  if test "${SYS}" = "mingw32"; then
    VLC_ADD_PLUGIN([waveout])
    VLC_ADD_LIBS([waveout],[-lwinmm])
  fi
  if test "${SYS}" = "mingwce"; then
    VLC_ADD_PLUGIN([waveout])
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
    [ VLC_ADD_PLUGIN([auhal])
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
    VLC_ADD_PLUGIN([hd1000a])
    AC_CHECK_LIB(HDMachineX225,main,VLC_ADD_LIBS([hd1000a],[-lHDMachineX225]))  ])
  AC_LANG_POP([C++])
fi

dnl
dnl  JACK modules
dnl
PKG_ENABLE_MODULES_VLC([JACK], [jack], [jack], [JACK audio I/O modules],[auto])
PKG_ENABLE_MODULES_VLC([JACK], [access_jack], [jack], [JACK audio I/O modules],[auto])


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
        VLC_ADD_PLUGIN([upnp_cc])
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
PKG_ENABLE_MODULES_VLC([UPNP], [upnp_intel], [libupnp], [Intel UPnp SDK],[auto])


dnl
dnl  Interface plugins
dnl

AC_ARG_WITH(,[Interface plugins:])

dnl special case for BeOS
if test "${SYS}" = "beos"
then
    VLC_ADD_PLUGIN([beos])
fi

dnl
dnl Skins2 module
dnl
AC_ARG_ENABLE(skins2,
  [AS_HELP_STRING([--enable-skins2],[Skins2 interface module (default
   enabled except on MacOSX, BeOS and WinCE)])])
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

  if test "${skins2_missing_lib}" = "no" && (test "${SYS}" = "mingw32"); then
    VLC_ADD_PLUGIN([skins2])
    ALIASES="${ALIASES} svlc"
    VLC_ADD_CPPFLAGS([skins2],[-U_OFF_T_ -U_off_t -Imodules/gui/skins2 -DWIN32_SKINS])
    VLC_ADD_CXXFLAGS([skins2],[-O2 -fno-rtti])
    VLC_ADD_LIBS([skins2],[-loleaut32 -lwinspool -lwinmm -lshell32 -lctl3d32 -ladvapi32 -lwsock32 -lgdi32 -lcomdlg32 -lole32 -luuid -lcomctl32])

  else if test "${skins2_missing_lib}" = "no" && (test "${SYS}" = "darwin"); then
    VLC_ADD_PLUGIN([skins2])
    ALIASES="${ALIASES} svlc"
    VLC_ADD_CPPFLAGS([skins2],[-Imodules/gui/skins2 -DMACOSX_SKINS])
    VLC_ADD_CXXFLAGS([skins2],[-O2 -fno-rtti])
    VLC_ADD_LDFLAGS([skins2],[-Wl,-framework,Carbon])

  else if test "${skins2_missing_lib}" = "no"; then
    VLC_ADD_PLUGIN([skins2])
    ALIASES="${ALIASES} svlc"
    VLC_ADD_CPPFLAGS([skins2],[-Imodules/gui/skins2 ${X_CFLAGS} -DX11_SKINS])
    VLC_ADD_CXXFLAGS([skins2],[-O2 -fno-rtti])
    VLC_ADD_LIBS([skins2],[${X_LIBS} ${X_PRE_LIBS} -lXext -lX11])
    need_xid_provider="no"
  fi fi fi
fi
AM_CONDITIONAL(BUILD_SKINS, [test "${enable_skins2}" = "yes" ||
	 (test "${SYS}" != "darwin" && test "${SYS}" != "beos" &&
	  test "${SYS}" != "mingwce" && test "${enable_skins2}" != "no")])

dnl
dnl Hildon UI
dnl
AC_ARG_ENABLE(hildon,
  [  --enable-hildon         Hildon touchscreen UI (default disabled)])
AS_IF([test "${enable_hildon}" = "yes"], [
  PKG_CHECK_MODULES(HILDON, [hildon-1 hildon-fm-2], [
    VLC_ADD_CFLAGS([hildon],[${HILDON_CFLAGS}])
    VLC_ADD_LIBS([hildon],[${HILDON_LIBS}])
    VLC_ADD_PLUGIN([hildon])
    ALIASES="${ALIASES} mvlc"
    need_xid_provider="no"
  ], [
    AS_IF([test "${enable_hildon}" = "yes"],[
      AC_MSG_ERROR([Hildon libraries not found])
    ])
    enable_hildon="no"
  ])
])
AM_CONDITIONAL(BUILD_HILDON, [test "${enable_hildon}" = "yes"])

dnl
dnl QT 4
dnl
AC_ARG_ENABLE(qt4,
  [  --enable-qt4            QT 4 support (default enabled) ],, [
  AS_IF([test "${SYS}" = "darwin"], [enable_qt4=no])
])
AS_IF([test "${enable_qt4}" != "no"], [
  PKG_CHECK_MODULES(QT4, [QtCore QtGui >= 4.4.0], [
    VLC_ADD_PLUGIN([qt4])
    ALIASES="${ALIASES} qvlc"
    AS_IF([test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce" -a "${SYS}" != "darwin"], [
        VLC_ADD_LIBS([qt4],[$QT4_LIBS -lX11])
        need_xid_provider="no"
    ], [
        VLC_ADD_LIBS([qt4],[$QT4_LIBS -lole32])
    ])
    QT4LOCALEDIR="$(eval $PKG_CONFIG --variable=prefix QtCore)/share/qt4/translations/"
    AC_SUBST(QT4LOCALEDIR)
    VLC_ADD_CXXFLAGS([qt4],[$QT4_CFLAGS -DQT4LOCALEDIR=\\\\\\\"${QT4LOCALEDIR}\\\\\\\"])
    AC_PATH_PROGS(MOC, [moc-qt4 moc], moc,`eval $PKG_CONFIG --variable=exec_prefix QtCore`/bin)
    AC_PATH_PROG(RCC, rcc, rcc,`eval $PKG_CONFIG --variable=exec_prefix QtCore`/bin)
    AC_PATH_PROGS(UIC, [uic-qt4 uic], uic,`eval $PKG_CONFIG --variable=exec_prefix QtCore`/bin)
  ], [
    AS_IF([test "${enable_qt4}" = "yes"],[
      AC_MSG_ERROR([Qt 4 library not found])
    ],[
      AC_MSG_WARN([Qt 4 library not found])
    ])
    enable_qt4="no"
  ])
])
AM_CONDITIONAL(ENABLE_QT4, [test "x$enable_qt4" != "xno"])

dnl
dnl  WinCE GUI module
dnl
AC_ARG_ENABLE(wince,
  [  --enable-wince          Windows CE interface (default enabled with MinGW)])
if test "${enable_wince}" != "no"; then
  if test "${SYS}" = "mingwce"; then
    VLC_ADD_PLUGIN([wince])
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
fi

dnl
dnl Simple test for skins2 dependency
dnl
AS_IF([test "${enable_skins2}" != "no" && test "x$enable_qt4" = "xno"], [
  AC_MSG_ERROR([The skins2 module depends on a the Qt4 development package. Without it you won't be able to open any dialog box from the interface, which makes the skins2 interface rather useless. Install the Qt4 development package or alternatively you can also configure with: --disable-qt4 --disable-skins2.])
])

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

     VLC_ADD_PLUGIN([opie])
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
  VLC_ADD_LDFLAGS([access_eyetv],                     [-Wl,-framework,Foundation])
  VLC_ADD_LDFLAGS([macosx minimal_macosx opengllayer qtcapture],[-Wl,-framework,Cocoa])
  VLC_ADD_LDFLAGS([macosx minimal_macosx opengllayer],[-Wl,-framework,OpenGL])
  VLC_ADD_LDFLAGS([macosx minimal_macosx],            [-Wl,-framework,Carbon])
  VLC_ADD_LDFLAGS([macosx minimal_macosx],            [-Wl,-framework,CoreServices])
  VLC_ADD_LDFLAGS([macosx minimal_macosx],            [-Wl,-framework,AGL])
  VLC_ADD_LDFLAGS([macosx],                           [-Wl,-framework,IOKit])
  VLC_ADD_LDFLAGS([macosx],                           [-F\\\${top_srcdir}/extras/contrib/Sparkle -Wl,-framework,Sparkle])
  VLC_ADD_OBJCFLAGS([macosx],                         [-F\\\${top_srcdir}/extras/contrib/Sparkle])
  dnl For bug report
  VLC_ADD_LDFLAGS([macosx],                           [-Wl,-framework,AddressBook])
  VLC_ADD_LDFLAGS([qtcapture],                        [-Wl,-framework,QTKit])
  VLC_ADD_LDFLAGS([qtcapture],                        [-Wl,-framework,CoreAudio])
  VLC_ADD_LDFLAGS([macosx],                           [-Wl,-framework,WebKit])
  VLC_ADD_LDFLAGS([opengllayer qtcapture],            [-Wl,-framework,QuartzCore])
  VLC_ADD_LDFLAGS([qtcapture],                        [-Wl,-framework,CoreVideo])
  VLC_ADD_OBJCFLAGS([macosx minimal_macosx opengllayer growl opengllayer], [-fobjc-exceptions] )

  VLC_ADD_PLUGIN([access_eyetv])
  VLC_ADD_PLUGIN([qtcapture])
  VLC_ADD_PLUGIN([macosx])
  VLC_ADD_PLUGIN([minimal_macosx])
  VLC_ADD_PLUGIN([opengllayer])
fi

dnl
dnl  QNX RTOS module
dnl
AC_ARG_ENABLE(qnx,
  [  --enable-qnx            QNX RTOS support (default enabled on QNX RTOS)])
    if test "${enable_qnx}" != "no"
    then
      AC_CHECK_HEADERS(Ph.h, [
        VLC_ADD_PLUGIN([qnx])
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
        [VLC_ADD_PLUGIN([ncurses])
        VLC_ADD_LIBS([ncurses],[-lncursesw])
        ALIASES="${ALIASES} nvlc"
        AC_DEFINE([HAVE_NCURSESW], 1, [Define to 1 if you have libncursesw.])
        AC_CHECK_LIB(ncursesw, tgetent, [],
          AC_CHECK_LIB(tinfow, tgetent, [VLC_ADD_LIBS([ncurses],[-ltinfow])],
            [AC_CHECK_LIB(tinfo, tgetent, [VLC_ADD_LIBS([ncurses],[-ltinfo])],
              [AS_IF([test "x${enable_ncurses}" != "x"],
                [AC_MSG_ERROR([tgetent not found in ncursesw tinfow tinfo]
               )])])
            ]
          )
        )
        ],
        [AC_CHECK_LIB( ncurses, mvprintw,
          [VLC_ADD_PLUGIN([ncurses])
          ALIASES="${ALIASES} nvlc"
          VLC_ADD_LIBS([ncurses],[-lncurses])
          AC_CHECK_LIB(ncurses, tgetent, [],
            [AC_CHECK_LIB(tinfo, tgetent, [VLC_ADD_LIBS([ncurses],[-ltinfo])],
              [AS_IF([test "x${enable_ncurses}" != "x"],
                [AC_MSG_ERROR([tgetent not found in ncurses tinfo])])]
            )]
          )],
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
  AC_CHECK_HEADERS(xosd.h,
    AC_CHECK_LIB(xosd, xosd_set_horizontal_offset, [
      VLC_ADD_PLUGIN([xosd])
      VLC_ADD_LIBS([xosd],[-lxosd])
    ])
  )
fi

dnl
dnl Framebuffer (overlay) plugin
dnl
AC_ARG_ENABLE(fbosd,
  [  --enable-fbosd          fbosd interface support (default disabled)])
if test "${enable_fbosd}" = "yes"
then
  AC_CHECK_HEADERS(linux/fb.h, [
    VLC_ADD_PLUGIN([fbosd])
 ])
fi

dnl
dnl Post-interface configuration checks
dnl
AS_IF([test "$need_xid_provider" = "yes"], [
  AC_MSG_ERROR([X11 video outputs need a window provider (Qt4, Skins2, Hildon or xcb-utils), but none were found. Please install xcb-keysyms.])
])

dnl
dnl Visualisation plugin
dnl
AC_ARG_ENABLE(visual,
  [  --enable-visual         visualisation plugin (default enabled)])
if test "${enable_visual}" != "no"
then
    VLC_ADD_PLUGIN([visual])
fi

dnl
dnl  goom visualization plugin
dnl
PKG_ENABLE_MODULES_VLC([GOOM], [], [libgoom2], [goom visualization plugin], [auto])

dnl
dnl libprojectM visualization plugin
dnl
PKG_ENABLE_MODULES_VLC([PROJECTM], [], [libprojectM], [projectM visualization plugin], [auto])

dnl
dnl  AtmoLight (homebrew AmbiLight)
dnl
AC_ARG_ENABLE(atmo,
AS_HELP_STRING([--disable-atmo],[AtmoLight (homebrew Philips ambilight)
                (broken, default disabled)]),, [enable_atmo="no"])
AS_IF([test "${enable_atmo}" != no], [
  AS_IF([test "${SYS}" = "mingw32" -o "${SYS}" = "linux"], [
    AC_LANG_PUSH(C++)
    VLC_ADD_PLUGIN([atmo])
    AC_LANG_POP(C++)
  ])
])

dnl
dnl  Bonjour services discovery
PKG_ENABLE_MODULES_VLC([BONJOUR], [], [avahi-client >= 0.6], [Bonjour services discovery], [auto])

dnl
dnl  libudev services discovery
PKG_ENABLE_MODULES_VLC([UDEV], [], [libudev >= 142], [Linux udev services discovery], [auto])

dnl
dnl MTP devices services discovery
PKG_ENABLE_MODULES_VLC([MTP], [], [libmtp >= 0.3.0],[MTP devices support],[auto])

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
    VLC_ADD_PLUGIN([lirc])
    VLC_ADD_LIBS([lirc],[-llirc_client])
  fi
fi


AC_ARG_WITH(,[Misc options:])

dnl
dnl libgcrypt
dnl
AC_ARG_ENABLE(libgcrypt,
  [  --disable-libgcrypt      gcrypt support (default enabled)])
AS_IF([test "${enable_libgcrypt}" != "no"], [
  AC_CHECK_DECL([GCRYCTL_SET_THREAD_CBS], [
    libgcrypt-config --version >/dev/null || \
        AC_MSG_ERROR([gcrypt.h present but libgcrypt-config could not be found])
    AC_CHECK_LIB(gcrypt, gcry_control, [
      have_libgcrypt="yes"
      GCRYPT_CFLAGS="`libgcrypt-config --cflags`"
      GCRYPT_LIBS="`libgcrypt-config --libs`"
    ], [
      AC_MSG_ERROR([libgcrypt not found. Install libgcrypt or use --disable-libgcrypt. Have a nice day.])
    ], [`libgcrypt-config --libs`])
  ], [
    AC_MSG_ERROR([libgcrypt version 1.1.94 or higher not found. Install libgcrypt or use --disable-libgcrypt. Have a nice day.])
  ], [#include <gcrypt.h>]
  )
])

if test "${SYS}" = "mingw32" -o "${SYS}" = "mingwce"; then
  AS_IF([test "${have_libgcrypt}" = "yes"],[
    VLC_ADD_LIBS([rtp stream_out_rtp], [${GCRYPT_LIBS}])
  ])
fi
AC_SUBST(GCRYPT_CFLAGS)
AC_SUBST(GCRYPT_LIBS)
AM_CONDITIONAL([HAVE_GCRYPT], [test "${have_libgcrypt}" = "yes"])

dnl
dnl TLS/SSL
dnl
AC_ARG_ENABLE(gnutls,
  [  --enable-gnutls         gnutls TLS/SSL support (default enabled)])

AS_IF([test "${enable_gnutls}" != "no"], [
  PKG_CHECK_MODULES(GNUTLS, [gnutls >= 1.7.4], [
    VLC_ADD_PLUGIN([gnutls])
    VLC_ADD_CFLAGS([gnutls], [$GNUTLS_CFLAGS])
    AS_IF([test "${SYS}" = "mingw32"], [
      dnl pkg-config --libs gnutls omits these
      VLC_ADD_LIBS([gnutls], [-lz])
      VLC_ADD_LIBS([gnutls], [${LTLIBINTL}])
    ])
    dnl The GnuTLS plugin invokes gcry_control directly.
    AS_IF([test "${have_libgcrypt}" = "yes"],[
      VLC_ADD_LIBS([gnutls], [${GCRYPT_LIBS}])
      VLC_ADD_CFLAGS([gnutls], [${GCRYPT_CFLAGS}])
    ])
    VLC_ADD_LIBS([gnutls], [$GNUTLS_LIBS])
  ], [
    AS_IF([test "${enable_gnutls}" = "yes"], [
      AC_MSG_ERROR([gnutls not present or too old (version 1.7.4 required)])
    ])
  ])
])


dnl
dnl RemoteOSD plugin (VNC client as video filter)
dnl
AC_ARG_ENABLE(remoteosd,
  [  --disable-remoteosd     RemoteOSD plugin (default enabled)])

AS_IF([test "${enable_remoteosd}" != "no"], [
  AS_IF([test "${have_libgcrypt}" = "yes"],[
    VLC_ADD_PLUGIN([remoteosd])
    VLC_ADD_LIBS([remoteosd], ${GCRYPT_LIBS})
    VLC_ADD_CFLAGS([remoteosd], ${GCRYPT_CFLAGS})
  ], [
    AC_MSG_ERROR([libgcrypt support required for RemoteOSD plugin])
  ])
])


dnl
dnl RAOP plugin
dnl
AC_MSG_CHECKING([whether to enable RAOP plugin])
AS_IF([test "${have_libgcrypt}" = "yes"], [
  AC_MSG_RESULT(yes)
  VLC_ADD_PLUGIN([stream_out_raop])
  VLC_ADD_LIBS([stream_out_raop], [${GCRYPT_LIBS} -lgpg-error])
  VLC_ADD_CFLAGS([stream_out_raop], [${GCRYPT_CFLAGS}])
], [
  AC_MSG_RESULT(no)
  AC_MSG_WARN([libgcrypt support required for RAOP plugin])
])


dnl
dnl OSSO (Maemo screen blanking) plugin
dnl
PKG_ENABLE_MODULES_VLC([OSSO], [], [libosso], [Maemo support], [auto])

AS_IF([test -f "/etc/maemo_version"], [
  AC_DEFINE([HAVE_MAEMO], 1, [Define to 1 if building for the Maemo platform.])
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
  VLC_ADD_LIBS([libvlccore], [${GCRYPT_LIBS}])
  VLC_ADD_CFLAGS([libvlccore], [${GCRYPT_CFLAGS}])
  AC_DEFINE([UPDATE_CHECK], 1, [Define if you want to use the VLC update mechanism])
fi

dnl
dnl SQLite
dnl
AC_ARG_ENABLE(sqlite,
  [  --enable-sqlite   SQLite (default disabled)],,
  [enable_sqlite = "no"])
AC_ARG_WITH(sqlite,
  [    --with-sqlite=PATH sqlite path linking])
if test "${SYS}" != "darwin"; then
  PKG_ENABLE_MODULES_VLC([SQLITE], [], [sqlite3], [sqlite3], [auto])
else
  if test "${enable_sqlite}" != "no"
  then
    AC_CHECK_HEADERS(sqlite3.h, [
        VLC_ADD_PLUGIN([sqlite])
        if test "${with_sqlite}" != "no" -a -n "${with_sqlite}"; then
          AC_MSG_CHECKING(existence of sqlite directory in ${with_sqlite})
          real_sqlite="`cd ${with_sqlite} 2>/dev/null && pwd`"
          if test -z "${real_sqlite}"
          then
            dnl  The given directory can't be found
            AC_MSG_RESULT(no)
            AC_MSG_ERROR([cannot cd to ${with_sqlite}])
          fi
          VLC_ADD_CFLAGS([sqlite],[-I${with_sqlite}/include])
          VLC_ADD_LIBS([sqlite], [-L${with_sqlite}/lib -lsqlite3])
          AC_MSG_RESULT(yes)
        else
          VLC_ADD_LIBS([sqlite], [-lsqlite3])
        fi
        AC_DEFINE([SQLITE_MODULE], 1, [Define if you want to use SQLite module]) ],
        AC_MSG_ERROR([sqlite3 is required for sqlite module]) )
  fi
fi
AM_CONDITIONAL([HAVE_SQLITE], [test "${enable_sqlite}" != "no"])


dnl
dnl  Endianness check
dnl
AC_C_BIGENDIAN
AS_IF([test "${ac_cv_c_bigendian}" = "yes"], [
  DEFS_BIGENDIAN="-DWORDS_BIGENDIAN=1"
], [
  DEFS_BIGENDIAN=""
])
AC_SUBST(DEFS_BIGENDIAN)

dnl
dnl  DLL loader copied from MPlayer copied from somewhere else (WINE ?)
dnl
loader=false
AC_ARG_ENABLE(loader,
  AS_HELP_STRING([--enable-loader],[build DLL loader for ELF i386 platforms
		  (default disabled)]))
AM_CONDITIONAL(LOADER, [test "${enable_loader}" = "yes"])
AS_IF([test "${enable_loader}" = "yes"],
  [ VLC_ADD_PLUGIN([dmo])
    VLC_ADD_CPPFLAGS([dmo],[-I\\\${top_srcdir}/libs/loader])
    VLC_ADD_LIBS([dmo],[\\\${top_builddir}/libs/loader/libloader.la -lpthread])
    VLC_ADD_CPPFLAGS([quicktime],[-I\\\${top_srcdir}/libs/loader])
    VLC_ADD_LIBS([quicktime],[\\\${top_builddir}/libs/loader/libloader.la -lpthread])
    VLC_ADD_CPPFLAGS([realaudio],[-I\\\${top_srcdir}/libs/loader -DLOADER])
    VLC_ADD_LIBS([realaudio],[\\\${top_builddir}/libs/loader/libloader.la])
    VLC_ADD_CPPFLAGS([realvideo],[-I\\\${top_srcdir}/libs/loader -DLOADER])
    VLC_ADD_LIBS([realvideo],[\\\${top_builddir}/libs/loader/libloader.la])
  ])

AC_ARG_WITH(,[Components:])

dnl
dnl  the VLC binary
dnl
AC_ARG_ENABLE(vlc,
  [  --enable-vlc            build the VLC media player (default enabled)])
AM_CONDITIONAL(BUILD_VLC, [test "${enable_vlc}" != "no"])

dnl
dnl  Microsoft ActiveX support
dnl
activex=false
AC_ARG_ENABLE(activex,
  AS_HELP_STRING([--enable-activex],[build a vlc-based ActiveX control
		  (default enabled on Win32)]))
AC_ARG_WITH(wine-sdk-path,
  [    --with-wine-sdk-path=PATH path to wine sdk])
if test "${enable_activex}" != "no"
then
  if test "${SYS}" = "mingw32"
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
  AS_HELP_STRING([--enable-mozilla],[build a vlc-based Firefox/Mozilla plugin
		  (default disabled)]))
AC_ARG_WITH(mozilla-sdk-path,
  [    --with-mozilla-sdk-path=PATH path to mozilla sdk])
AC_ARG_WITH(mozilla-pkg,
  [    --with-mozilla-pkg=PKG  look for PKG.pc to build the mozilla plugin.])
AC_LANG_PUSH(C++)
if test "${enable_mozilla}" = "yes"
then
  AS_IF([test "${with_mozilla_sdk_path}" = "" -o "${with_mozilla_sdk_path}" = "no"],
    [
    dnl pkg-config
    dnl As we want to do a loop due to the number of name possible for the .pc
    dnl we can't use the pkg-config macros.

    AC_ARG_VAR([MOZILLA_CFLAGS], [C compiler flags for Mozilla, overriding pkg-config])
    AC_ARG_VAR([MOZILLA_LIBS], [linker flags for Mozilla, overriding pkg-config])
    AC_MSG_NOTICE([Checking for Mozilla])
    found=0
    if test -n "$MOZILLA_CFLAGS" -a -n "$MOZILLA_LIBS"
    then
      found=1
    else
      if test -n "$PKG_CONFIG"; then
	for i in "${with_mozilla_pkg}" libxul {seamonkey,iceape,xulrunner,firefox,iceweasel,mozilla}-plugin; do
	  echo "Trying to find $i.pc files" >&5
	  if $PKG_CONFIG --exists --print-errors "$i" 2>&5
	  then
            echo "Using $i.pc files." >&5
            echo "Using $i.pc files." >&6
	    found=1
	    MOZILLA_CFLAGS=$(eval $PKG_CONFIG --cflags "$i" )
	    MOZILLA_LIBS=$(eval $PKG_CONFIG --libs "$i" )
	    break
	  fi
	done
      fi
    fi
    AS_IF( [test $found = 1],[
        CPPFLAGS="${CPPFLAGS_save} ${MOZILLA_CFLAGS}"
        MOZILLA_REQUIRED_HEADERS=1
        AC_CHECK_HEADERS([npfunctions.h])
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
        AS_IF([ test "${SYS}" != "mingw32" -a "${SYS}" != "mingwce"],[
	   VLC_ADD_CPPFLAGS([mozilla],[-DXP_UNIX -DOJI])
	   AS_IF([ test "${SYS}" != "darwin"],[
	     PKG_CHECK_MODULES(XPM, [xpm xt],[
		VLC_ADD_CPPFLAGS([mozilla],[-DMOZ_X11])
	     ],[
		AC_MSG_ERROR([Please install the libXpm and libXt development files.])
	     ])
          ])
        ])
	VLC_ADD_CPPFLAGS([mozilla],[${CPPFLAGS} ${XPM_CFLAGS}])
        VLC_ADD_LIBS([mozilla],[${MOZILLA_LIBS} ${XPM_LIBS}])
        VLC_ADD_PLUGIN([mozilla])
        PLUGINS_BINDINGS="${PLUGINS_BINDINGS} mozilla"
        MOZILLA_CONFIG=
        CPPFLAGS="${CPPFLAGS_save}"
          ],
          [
            AC_PATH_PROGS(MOZILLA_CONFIG,
	    [mozilla-config seamonkey-config xulrunner-config],
        [no])
            test "${MOZILLA_CONFIG}" = "no" && AC_MSG_ERROR([Please install the Mozilla development tools. mozilla-config was not found.])
          ]
        )
    dnl pkg-config failed but we might have found a mozilla-config
    AS_IF( [test ! -z "${MOZILLA_CONFIG}"],[
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
      VLC_ADD_CPPFLAGS([mozilla],[[`${MOZILLA_CONFIG} --cflags plugin java`]])
      VLC_ADD_LIBS([mozilla],[`${MOZILLA_CONFIG} --libs plugin`])
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
    ])
    dnl End of moz_sdk = ""
  ],[
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
 ])
fi
AC_LANG_POP(C++)
AM_CONDITIONAL(BUILD_MOZILLA,${mozilla})

dnl Tests for Xsp
AC_CHECK_LIB(Xsp, XSPSetPixelDoubling,[
  VLC_ADD_CPPFLAGS([xvmc glx],[-DHAVE_XSP])
  VLC_ADD_LIBS([xvmc glx],[-lXsp])
])

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

  VLC_ADD_PLUGIN([test1])
  VLC_ADD_PLUGIN([test2])
  VLC_ADD_PLUGIN([test3])
  VLC_ADD_PLUGIN([test4])
fi

dnl
dnl  qte_main plugin
dnl
if test "${NEED_QTE_MAIN}" != "no"
then
    VLC_ADD_PLUGIN([qte_main])
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

AS_IF([test "${plugin_support}" != "no"], [
  AC_DEFINE(HAVE_DYNAMIC_PLUGINS, 1, [Define if dynamic plugins are supported])
])

dnl
dnl Pic and shared libvlc stuff
dnl
AS_IF([test "${SYS}" = "mingw32"], [
  FILE_LIBVLCCORE_DLL="!define LIBVLCCORE_DLL libvlccore.dll"
  FILE_LIBVLC_DLL="!define LIBVLC_DLL libvlc.dll"
])

dnl
dnl  Stuff used by the program
dnl
VERSION_MESSAGE="${VERSION} ${CODENAME}"
COPYRIGHT_MESSAGE="VLC media player - version ${VERSION_MESSAGE} - (c) ${COPYRIGHT_YEARS} the VideoLAN team"
AC_DEFINE_UNQUOTED(VERSION_MESSAGE, "${VERSION_MESSAGE}", [Simple version string])
AC_DEFINE_UNQUOTED(COPYRIGHT_MESSAGE, "${COPYRIGHT_MESSAGE}", [Copyright string])
AC_DEFINE_UNQUOTED(COPYRIGHT_YEARS, "${COPYRIGHT_YEARS}", [The copyright years])
AC_DEFINE_UNQUOTED(CONFIGURE_LINE, "${CONFIGURE_LINE}", [The ./configure command line])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_MAJOR,"${VERSION_MAJOR}", [version major number])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_MINOR,"${VERSION_MINOR}", [version minor number])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_REVISION,"${VERSION_REVISION}", [version minor number])
AC_DEFINE_UNQUOTED(PACKAGE_VERSION_EXTRA,"${VERSION_EXTRA}", [version minor number])
AC_SUBST(COPYRIGHT_MESSAGE)
AC_SUBST(VERSION_MESSAGE)
AC_SUBST(VERSION_MAJOR)
AC_SUBST(VERSION_MINOR)
AC_SUBST(VERSION_REVISION)
AC_SUBST(VERSION_EXTRA)
AC_SUBST(COPYRIGHT_YEARS)
AC_DEFINE_UNQUOTED(VLC_COMPILE_BY, "`whoami`", [user who ran configure])
if test "${build_os}" = "cygwin"
then
    AC_DEFINE_UNQUOTED(VLC_COMPILE_HOST, "`hostname`", [host which ran configure])
else
    AC_DEFINE_UNQUOTED(VLC_COMPILE_HOST, "`hostname -s`", [host which ran configure])
fi
AC_DEFINE_UNQUOTED(VLC_COMPILE_DOMAIN, "`dnsdomainname 2>/dev/null || domainname 2>/dev/null || echo unknown`", [domain of the host which ran configure])
AC_DEFINE_UNQUOTED(VLC_COMPILER, "`$CC -v 2>&1 | tail -n 1`", [compiler])
dnl Win32 need s a numerical version_extra.
case $( echo ${VERSION_EXTRA}|wc -m ) in
       "1") VERSION_EXTRA_RC="0";;
       "2") VERSION_EXTRA_RC=$( echo ${VERSION_EXTRA}|tr "abcdefghi" "123456789") ;;
       *) VERSION_EXTRA_RC="99"
esac
AC_SUBST(VERSION_EXTRA_RC)
dnl
dnl  Handle substvars that use $(top_srcdir)
dnl
VLC_CONFIG="top_srcdir=\"\$(top_srcdir)\" top_builddir=\"\$(top_builddir)\" \$(top_builddir)/vlc-config"
AC_SUBST(VLC_CONFIG)
CPPFLAGS_save="-I\$(top_srcdir)/include -I\$(top_builddir)/include ${CPPFLAGS_save}"

dnl
dnl  Restore *FLAGS
dnl
VLC_RESTORE_FLAGS

dnl
dnl Sort the modules list
dnl
PLUGINS=$( (for i in `echo $PLUGINS`; do echo $i; done)|sort|xargs )

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
AC_SUBST(FILE_LIBVLCCORE_DLL)
AC_SUBST(FILE_LIBVLC_DLL)

dnl Create vlc-config.in
VLC_OUTPUT_VLC_CONFIG_IN

AC_CONFIG_FILES([
  extras/package/win32/vlc.win32.nsi
  extras/package/macosx/Info.plist
  extras/package/macosx/Resources/English.lproj/InfoPlist.strings
  extras/package/macosx/plugin/Info.plist
  extras/package/macosx/plugin/InstallerInfo.plist
  Makefile
  projects/activex/Makefile
  projects/activex/axvlc.inf
  doc/Makefile
  extras/package/ipkg/Makefile
  libs/loader/Makefile
  libs/srtp/Makefile
  libs/unzip/Makefile
  modules/Makefile
  projects/mozilla/Makefile
  m4/Makefile
  po/Makefile.in
  projects/activex/axvlc_rc.rc
  projects/mozilla/npvlc_rc.rc
  projects/mozilla/vlc.r
  projects/mozilla/install.js
  share/Makefile
  share/vlc_win32_rc.rc
  share/libvlc_win32_rc.rc
  compat/Makefile
  src/Makefile
  src/test/Makefile
  bin/Makefile
  test/Makefile
  modules/access/Makefile
  modules/access/bd/Makefile
  modules/access/bda/Makefile
  modules/access/dshow/Makefile
  modules/access/dvb/Makefile
  modules/access/mms/Makefile
  modules/access/cdda/Makefile
  modules/access/rtp/Makefile
  modules/access/rtsp/Makefile
  modules/access/vcd/Makefile
  modules/access/vcdx/Makefile
  modules/access/screen/Makefile
  modules/access/zip/Makefile
  modules/access_output/Makefile
  modules/audio_filter/Makefile
  modules/audio_filter/channel_mixer/Makefile
  modules/audio_filter/converter/Makefile
  modules/audio_filter/resampler/Makefile
  modules/audio_filter/spatializer/Makefile
  modules/audio_mixer/Makefile
  modules/audio_output/Makefile
  modules/codec/Makefile
  modules/codec/avcodec/Makefile
  modules/codec/cmml/Makefile
  modules/codec/dmo/Makefile
  modules/codec/shine/Makefile
  modules/codec/subtitles/Makefile
  modules/codec/spudec/Makefile
  modules/codec/wmafixed/Makefile
  modules/codec/xvmc/Makefile
  modules/control/Makefile
  modules/control/http/Makefile
  modules/control/globalhotkeys/Makefile
  modules/demux/Makefile
  modules/demux/asf/Makefile
  modules/demux/avformat/Makefile
  modules/demux/avi/Makefile
  modules/demux/mkv/Makefile
  modules/demux/mp4/Makefile
  modules/demux/mpeg/Makefile
  modules/demux/playlist/Makefile
  modules/gui/Makefile
  modules/gui/beos/Makefile
  modules/gui/macosx/Makefile
  modules/gui/hildon/Makefile
  modules/gui/minimal_macosx/Makefile
  modules/gui/qnx/Makefile
  modules/gui/qt4/Makefile
  modules/gui/skins2/Makefile
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
  modules/misc/stats/Makefile
  modules/misc/xml/Makefile
  modules/mux/Makefile
  modules/mux/mpeg/Makefile
  modules/packetizer/Makefile
  modules/services_discovery/Makefile
  modules/stream_filter/Makefile
  modules/stream_out/Makefile
  modules/stream_out/transcode/Makefile
  modules/video_chroma/Makefile
  modules/video_filter/Makefile
  modules/video_filter/dynamicoverlay/Makefile
  modules/video_output/Makefile
  modules/video_output/msw/Makefile
  modules/video_output/qte/Makefile
  modules/video_output/x11/Makefile
  modules/visualization/Makefile
  modules/visualization/visual/Makefile
])

dnl Generate makefiles
AC_OUTPUT

# Cannot use AC_CONFIG_FILES([vlc-config]) as is automatically built,
# not provided with the source
${SHELL} ./config.status --file=vlc-config
chmod 0755 vlc-config

/bin/echo -n "Enabled modules: "
./vlc-config --list plugin

dnl Do we have to use make or gmake ?
USE_MAKE_OR_GMAKE=`case "${SYS}" in openbsd*) echo "gmake";; *) echo "make";; esac`
dnl Shortcut to nice compile message
rm -f compile
echo '#! /bin/sh' >compile
echo rm -f .error\$\$ >>compile
echo ERROR=0 >>compile
echo export PATH=$PATH LANG=C >>compile
echo "($USE_MAKE_OR_GMAKE V=1 \$@ 2>&1 || touch .error\$\$)| \\" >>compile
echo '`sed -ne "s/^top_srcdir *= *//p" < Makefile`/extras/buildsystem/make.pl' >>compile
echo test -f .error\$\$ \&\& ERROR=1 >>compile
echo rm -f .error\$\$ >>compile
echo exit \$ERROR >>compile
chmod a+x compile

printf "
libvlc configuration
--------------------
version               : ${VERSION}
system                : ${SYS}
architecture          : ${ARCH}
build flavour         : "
test "${enable_debug}" = "yes" && printf "debug "
test "${enable_cprof}" = "yes" && printf "cprof "
test "${enable_gprof}" = "yes" && printf "gprof "
test "${enable_optimizations}" = "yes" && printf "optim "
test "${enable_release}" = "yes" && printf "release " || printf "devel "
echo ""
if test "${enable_vlc}" != "no"; then
echo "vlc aliases           :${ALIASES}"
else
echo "build vlc executable  : no"
fi
echo "plugins/bindings      :${PLUGINS_BINDINGS}

You can tune the compiler flags in vlc-config.
To build vlc and its plugins, type \`./compile' or \`$USE_MAKE_OR_GMAKE'.
"
if test "x$ac_ld_does_not_support_text_reloc" = "xyes"; then
   echo ""
   echo "Warning: Due to a bug in ld, mmx/sse support has been"
   echo "         turned off."
   echo "         FFmpeg will be REALLY slow."
   echo "         VLC WILL NOT PERFORM AS EXPECTED."
   echo ""
fi
