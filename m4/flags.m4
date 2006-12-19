# <rdenis (at) simphalempin (dot) com>.
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
  AS_IF([test "${ac_res}" != "no"], [
    CFLAGS="${CFLAGS_save} $1"
    $2
  ], [
    CFLAGS="${CFLAGS_save}"
    $3
  ])
])

AC_DEFUN([RDC_PROG_CC_WFLAGS],
[ for a in $1; do
    RDC_PROG_CC_FLAGS_IFELSE([-W$a])
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
