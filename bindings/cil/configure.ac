dnl configure.ac - Configure script for libvlc-cil

dnl Process this file with GNU Autoconf to produce a configure script

dnl ***********************************************************************
dnl *  Copyright © 2007 Rémi Denis-Courmont.                              *
dnl *  This program is free software; you can redistribute and/or modify  *
dnl *  it under the terms of the GNU General Public License as published  *
dnl *  by the Free Software Foundation; version 2 of the license, or (at  *
dnl *  your option) any later version.                                    *
dnl *                                                                     *
dnl *  This program is distributed in the hope that it will be useful,    *
dnl *  but WITHOUT ANY WARRANTY; without even the implied warranty of     *
dnl *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.               *
dnl *  See the GNU General Public License for more details.               *
dnl *                                                                     *
dnl *  You should have received a copy of the GNU General Public License  *
dnl *  along with this program; if not, you can get it from:              *
dnl *  http://www.gnu.org/copyleft/gpl.html                               *
dnl ***********************************************************************

AC_COPYRIGHT([Copyright (C) 2004-2009 Remi Denis-Courmont])
AC_INIT(libvlc-cil, 1.0.0, vlc-devel_no_bulk_mail@videolan.org)
AC_PREREQ(2.50)

AC_CONFIG_SRCDIR(configure.ac)
AC_CONFIG_AUX_DIR(admin)
AC_CONFIG_MACRO_DIR(m4)

AC_ARG_VAR([CSC], [C sharp compiler command])
AC_ARG_VAR([CSFLAGS], [C sharp compiler flags])
AC_PATH_PROG([CSC], [gmcs csc], [false])

AC_ARG_VAR([DOXYGEN], [Doxygen command])
AC_PATH_PROG([DOXYGEN], [doxygen], [false])

AM_INIT_AUTOMAKE

AC_CONFIG_FILES([Makefile m4/Makefile src/Makefile tests/Makefile Doxyfile])
AC_OUTPUT
