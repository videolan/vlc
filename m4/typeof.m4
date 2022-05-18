dnl @synopsis AX_CXX_TYPEOF
dnl
dnl tests for the presence of the gcc hashmap stl extension
dnl
dnl @author Patrick Mauritz <oxygene@studentenbude.ath.cx>
dnl @version 2005-08-15
dnl @license AllPermissive

AC_DEFUN([AX_CXX_TYPEOF],[
AC_LANG_ASSERT([C++])
AC_CACHE_CHECK(whether the C++ compiler supports typeof,
ac_cv_cxx_typeof,
[AC_COMPILE_IFELSE([AC_LANG_SOURCE([[ int x; typeof (x) y[6]; ]])],
  [ac_cv_cxx_typeof=yes],
  [ac_cv_cxx_typeof=no])
])
if test "$ac_cv_cxx_typeof" = yes; then
  AC_DEFINE(HAVE_CXX_TYPEOF, 1, [Define if the compiler supports typeof.])
fi 
])
