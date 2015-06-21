# Copyright © 2015 Rémi Denis-Courmont
# This file (c11.m4) is free software; unlimited permission to
# copy and/or distribute it , with or without modifications, as long
# as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.


AC_DEFUN([VLC_PROG_CC_C11], [
  AC_LANG_ASSERT(C)

    for opt in "" -std=gnu11 -std=c11 -c11
    do
      cachevar=AS_TR_SH([ax_cv_c_compile_c11_$opt])
      AC_CACHE_CHECK([whether $CC $opt supports C11], [$cachevar], [
        CFLAGS_save="$CFLAGS"
        CFLAGS="$CFLAGS $opt"
        dnl PREPROC is not enough due to CFLAGS usage
        AC_COMPILE_IFELSE([AC_LANG_SOURCE([
[#ifndef __STDC_VERSION__
# error Not a C compiler!
#endif
#if (__STDC_VERSION__ < 201112L)
# error Too old C compiler!
#endif
_Static_assert(1, "Not C11!");
const int varname = _Generic(1, int: 1, default: 0);
]])], [
          eval $cachevar="yes"
        ], [
          eval $cachevar="no"
        ])
        CFLAGS="$CFLAGS_save"
      ])
      if eval test "\$$cachevar" = "yes"
      then
        CFLAGS="$CFLAGS $opt"
        break
      fi
    done
])
