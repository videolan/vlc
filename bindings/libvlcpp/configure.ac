dnl configure.ac - Configure script for libvlcpp

AC_COPYRIGHT([Copyright (C) 2010 Rémi Duraffort])
AC_INIT(libvlcpp, 1.0.0, vlc-devel@no-spam@videolan.org)
AC_PREREQ([2.65])

AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_AUX_DIR(admin)
AC_CONFIG_MACRO_DIR([m4])

dnl Checks for programs.
AC_PROG_CXX
AM_PROG_LIBTOOL

AC_ARG_VAR([CXXFLAGS], [C++ compiler flag])

dnl Checks for vlc library
PKG_CHECK_MODULES([libvlc], [libvlc >= 1.1.0])
AC_SUBST([libvlc_CFLAGS])
AC_SUBST([libvlc_LIBS])

AC_ARG_VAR([DOXYGEN], [Doxygen command])
AC_PATH_PROG([DOXYGEN], [doxygen], [false])

AM_INIT_AUTOMAKE

AC_CONFIG_FILES([Makefile src/Makefile src/libvlcpp.pc Doxyfile])
AC_OUTPUT
