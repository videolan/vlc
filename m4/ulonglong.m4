# ulonglong.m4 serial 6
dnl Copyright (C) 1999-2006 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl From Paul Eggert.

# Define HAVE_UNSIGNED_LONG_LONG_INT if 'unsigned long long int' works.
# This fixes a bug in Autoconf 2.60, but can be removed once we
# assume 2.61 everywhere.

# Note: If the type 'unsigned long long int' exists but is only 32 bits
# large (as on some very old compilers), AC_TYPE_UNSIGNED_LONG_LONG_INT
# will not be defined. In this case you can treat 'unsigned long long int'
# like 'unsigned long int'.

AC_DEFUN([AC_TYPE_UNSIGNED_LONG_LONG_INT],
[
  AC_CACHE_CHECK([for unsigned long long int],
    [ac_cv_type_unsigned_long_long_int],
    [AC_LINK_IFELSE(
       [AC_LANG_PROGRAM(
	  [[unsigned long long int ull = 18446744073709551615ULL;
	    typedef int a[(18446744073709551615ULL <= (unsigned long long int) -1
			   ? 1 : -1)];
	   int i = 63;]],
	  [[unsigned long long int ullmax = 18446744073709551615ull;
	    return (ull << 63 | ull >> 63 | ull << i | ull >> i
		    | ullmax / ull | ullmax % ull);]])],
       [ac_cv_type_unsigned_long_long_int=yes],
       [ac_cv_type_unsigned_long_long_int=no])])
  if test $ac_cv_type_unsigned_long_long_int = yes; then
    AC_DEFINE([HAVE_UNSIGNED_LONG_LONG_INT], 1,
      [Define to 1 if the system has the type `unsigned long long int'.])
  fi
])

# This macro is obsolescent and should go away soon.
AC_DEFUN([gl_AC_TYPE_UNSIGNED_LONG_LONG],
[
  AC_REQUIRE([AC_TYPE_UNSIGNED_LONG_LONG_INT])
  ac_cv_type_unsigned_long_long=$ac_cv_type_unsigned_long_long_int
  if test $ac_cv_type_unsigned_long_long = yes; then
    AC_DEFINE(HAVE_UNSIGNED_LONG_LONG, 1,
      [Define if you have the 'unsigned long long' type.])
  fi
])
