# Copyright © 2006 Rémi Denis-Courmont
# This file (flags.m4) is free software; unlimited permission to
# copy and/or distribute it , with or without modifications, as long
# as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.

AC_DEFUN([RDC_PROG_CC_FLAGS_IFELSE],
[AC_LANG_ASSERT(C)
  CFLAGS_save="${CFLAGS}"
  as_ac_var=`echo "ac_cv_prog_cc_flags_$1" | $as_tr_sh`
  AC_CACHE_CHECK([if $CC accepts $1], [$as_ac_var], [
    CFLAGS="${CFLAGS} $1"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
      eval "$as_ac_var=yes"
    ],[
      eval "$as_ac_var=no"
    ])
  ])

  ac_res=`eval echo '${'$as_ac_var'}'`
  CFLAGS="${CFLAGS_save}"
  AS_IF([test "${ac_res}" != "no"], [$2], [$3])
])

AC_DEFUN([RDC_PROG_CC_FLAGS],
[AC_LANG_ASSERT(C)
  RDC_PROG_CC_FLAGS_IFELSE([$1], [CFLAGS="${CFLAGS} $1"])
])

AC_DEFUN([RDC_PROG_CC_WFLAGS],
[ for a in $1; do
    RDC_PROG_CC_FLAGS([-W$a])
  done
])

AC_DEFUN([RDC_PROG_CXX_FLAGS_IFELSE],
[AC_LANG_ASSERT(C++)
  CXXFLAGS_save="${CXXFLAGS}"
  as_ac_var=`echo "ac_cv_prog_cxx_flags_$1" | $as_tr_sh`
  AC_CACHE_CHECK([if $CXX accepts $1], [$as_ac_var], [
    CXXFLAGS="${CXXFLAGS} $1"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [
      eval "$as_ac_var=yes"
    ],[
      eval "$as_ac_var=no"
    ])
  ])

  ac_res=`eval echo '${'$as_ac_var'}'`
  CXXFLAGS="${CXXFLAGS_save}"
  AS_IF([test "${ac_res}" != "no"], [$2], [$3])
])

AC_DEFUN([RDC_PROG_CXX_FLAGS],
[AC_LANG_ASSERT(C++)
  RDC_PROG_CXX_FLAGS_IFELSE([$1], [CXXFLAGS="${CXXFLAGS} $1"])
])

AC_DEFUN([RDC_PROG_CXX_WFLAGS],
[ for a in $1; do
    RDC_PROG_CXX_FLAGS([-W$a])
  done
])

AC_DEFUN([RDC_PROG_LINK_FLAGS_IFELSE],
[AC_LANG_ASSERT(C)
  LDFLAGS_save="${LDFLAGS}"
  as_ac_var=`echo "ac_cv_prog_link_flags_$1" | $as_tr_sh`
  AC_CACHE_CHECK([if $LINK accepts $1], [$as_ac_var], [
    LDFLAGS="${LDFLAGS} $1"
    AC_LINK_IFELSE([AC_LANG_PROGRAM()], [
      eval "$as_ac_var=yes"
    ],[
      eval "$as_ac_var=no"
    ])
  ])

  ac_res=`eval echo '${'$as_ac_var'}'`
  AS_IF([test "${ac_res}" != "no"], [
    LDFLAGS="${LDFLAGS} $1"
    $2
  ], [
    LDFLAGS="${LDFLAGS_save}"
    $3
  ])
])
