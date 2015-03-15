dnl  Macros needed for VLC

dnl ===========================================================================
dnl  Macros to add plugins or builtins and handle their flags
m4_pattern_allow([^PKG_CONFIG(_LIBDIR)?$])


AC_DEFUN([VLC_ADD_PLUGIN], [
  m4_foreach_w([element], [$1], [
    AC_SUBST([LTLIB]element, [lib]element[_plugin.la])
  ])
])

AC_DEFUN([VLC_ADD_CPPFLAGS], [
  m4_foreach_w([element], [$1], [
    [eval "CPPFLAGS_]element[="'"$'"{CPPFLAGS_]element[} $2"'"']
    AC_SUBST([CPPFLAGS_]element)
  ])
])

AC_DEFUN([VLC_ADD_CFLAGS], [
  m4_foreach_w([element], [$1], [
    [eval "CFLAGS_]element[="'"$'"{CFLAGS_]element[} $2"'"']
    AC_SUBST([CFLAGS_]element)
  ])
])

AC_DEFUN([VLC_ADD_CXXFLAGS], [
  m4_foreach_w([element], [$1], [
    [eval "CXXFLAGS_]element[="'"$'"{CXXFLAGS_]element[} $2"'"']
    AC_SUBST([CXXFLAGS_]element)
  ])
])

AC_DEFUN([VLC_ADD_OBJCFLAGS], [
  m4_foreach_w([element], [$1], [
    [eval "OBJCFLAGS_]element[="'"$'"{OBJCFLAGS_]element[} $2"'"']
    AC_SUBST([OBJCFLAGS_]element)
  ])
])

AC_DEFUN([VLC_ADD_LDFLAGS], [
  m4_foreach_w([element], [$1], [
    [eval "LDFLAGS_]element[="'"$2 $'"{LDFLAGS_]element[}"'"']
    AC_SUBST([LDFLAGS_]element)
  ])
])

AC_DEFUN([VLC_ADD_LIBS], [
  m4_foreach_w([element], [$1], [
    [eval "LIBS_]element[="'"'"$2 "'$'"{LIBS_]element[}"'"']
    AC_SUBST([LIBS_]element)
  ])
])

dnl ===========================================================================
dnl  Macros to save and restore default flags

AC_DEFUN([VLC_SAVE_FLAGS], [
  CPPFLAGS_save="${CPPFLAGS}"
  CFLAGS_save="${CFLAGS}"
  CXXFLAGS_save="${CXXFLAGS}"
  OBJCFLAGS_save="${OBJCFLAGS}"
  LDFLAGS_save="${LDFLAGS}"
  LIBS_save="${LIBS}"
])

AC_DEFUN([VLC_RESTORE_FLAGS], [
  CPPFLAGS="${CPPFLAGS_save}"
  CFLAGS="${CFLAGS_save}"
  CXXFLAGS="${CXXFLAGS_save}"
  OBJCFLAGS="${OBJCFLAGS_save}"
  LDFLAGS="${LDFLAGS_save}"
  LIBS="${LIBS_save}"
])

dnl ===========================================================================
dnl  Macros for shared object handling (TODO)

AC_DEFUN([VLC_LIBRARY_SUFFIX], [
  AC_MSG_CHECKING(for shared objects suffix)
  case "${host_os}" in
    darwin*)
      LIBEXT=".dylib"
      ;;
    *mingw32* | *cygwin* | *wince* | *mingwce* | *os2*)
      LIBEXT=".dll"
      ;;
    hpux*)
      LIBEXT=".sl"
      ;;
    *)
      LIBEXT=".so"
      ;;
  esac
  AC_MSG_RESULT(${LIBEXT})
  AC_DEFINE_UNQUOTED(LIBEXT, "${LIBEXT}", [Dynamic object extension])
])

dnl ===========================================================================
dnl  Custom macros for checking functions with inline fallback, for mingw32/64

dnl VLC_REPLACE_DECL([funcname], [include])
AC_DEFUN([VLC_REPLACE_DECL], [
  AS_VAR_PUSHDEF([CACHEVAR], [vlc_cv_replace_decl] AS_TR_SH([$1]))
  AC_CACHE_VAL(CACHEVAR, [
    AC_CHECK_DECL(
      [$1],
      AS_VAR_SET(CACHEVAR, [yes]),
      AS_VAR_SET(CACHEVAR, [no]),
      [$2]
    )
  ])
  AS_IF([test x"AS_VAR_GET(CACHEVAR)" = xyes],
    [AC_DEFINE(AS_TR_CPP([HAVE_$1]), [1], [Define to 1 if you have $1 function])],
    [AC_LIBOBJ([$1])])
  AS_VAR_POPDEF([CACHEVAR])
])
