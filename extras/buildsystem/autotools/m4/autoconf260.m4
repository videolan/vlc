# Replacements for autoconf 2.59 and older
# Please make official source tarballs with recent autoconf only.
# Copyright (C) 2006 Rémi Denis-Courmont.
# Distribution and usage of this file, verbatim or modified, is permitted
# with no limitations.

AC_DEFUN([AC_PROG_CC_C99],
[ AC_REQUIRE([AC_PROG_CC])dnl
  CC="$CC -std=gnu99"
])

AC_DEFUN([AC_PROG_OBJC], [ 
 AC_MSG_CHECKING(for an Objective-C compiler) 	 	 
 OBJC="${CXX}" 	 	 
 AC_SUBST(OBJC) 	 	 
 OBJCFLAGS="${CXXFLAGS} -fgnu-runtime -fconstant-string-class=NSConstantString" 	 	 
 AC_SUBST(OBJCFLAGS)

])

AC_DEFUN([AC_USE_SYSTEM_EXTENSIONS],
[ AC_DEFINE([_GNU_SOURCE], [ ], [Enable lots of stuff with glibc.])
])

