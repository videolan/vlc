# Replacements for autoconf 2.59 and older
# Please make official source tarballs with recent autoconf only.

AC_DEFUN([AC_PROG_CC_C00],
[ AC_REQUIRE([AC_PROG_CC])dnl
  CC="$CC -std=c99"
])
