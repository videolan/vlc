# fcntl-o.m4 serial 1
dnl Copyright (C) 2006, 2009-2010 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

dnl Written by Paul Eggert.

# Test whether the flags O_NOATIME and O_NOFOLLOW actually work.
# Define HAVE_WORKING_O_NOATIME to 1 if O_NOATIME works, or to 0 otherwise.
# Define HAVE_WORKING_O_NOFOLLOW to 1 if O_NOFOLLOW works, or to 0 otherwise.
AC_DEFUN([gl_FCNTL_O_FLAGS],
[
  dnl Persuade glibc <fcntl.h> to define O_NOATIME and O_NOFOLLOW.
  AC_REQUIRE([AC_USE_SYSTEM_EXTENSIONS])
  AC_CACHE_CHECK([for working fcntl.h], [gl_cv_header_working_fcntl_h],
    [AC_RUN_IFELSE(
       [AC_LANG_PROGRAM(
          [[#include <sys/types.h>
           #include <sys/stat.h>
           #include <unistd.h>
           #include <fcntl.h>
           #ifndef O_NOATIME
            #define O_NOATIME 0
           #endif
           #ifndef O_NOFOLLOW
            #define O_NOFOLLOW 0
           #endif
           static int const constants[] =
            {
              O_CREAT, O_EXCL, O_NOCTTY, O_TRUNC, O_APPEND,
              O_NONBLOCK, O_SYNC, O_ACCMODE, O_RDONLY, O_RDWR, O_WRONLY
            };
          ]],
          [[
            int status = !constants;
            {
              static char const sym[] = "conftest.sym";
              if (symlink (".", sym) != 0
                  || close (open (sym, O_RDONLY | O_NOFOLLOW)) == 0)
                status |= 32;
              unlink (sym);
            }
            {
              static char const file[] = "confdefs.h";
              int fd = open (file, O_RDONLY | O_NOATIME);
              char c;
              struct stat st0, st1;
              if (fd < 0
                  || fstat (fd, &st0) != 0
                  || sleep (1) != 0
                  || read (fd, &c, 1) != 1
                  || close (fd) != 0
                  || stat (file, &st1) != 0
                  || st0.st_atime != st1.st_atime)
                status |= 64;
            }
            return status;]])],
       [gl_cv_header_working_fcntl_h=yes],
       [case $? in #(
        32) gl_cv_header_working_fcntl_h='no (bad O_NOFOLLOW)';; #(
        64) gl_cv_header_working_fcntl_h='no (bad O_NOATIME)';; #(
        96) gl_cv_header_working_fcntl_h='no (bad O_NOATIME, O_NOFOLLOW)';; #(
         *) gl_cv_header_working_fcntl_h='no';;
        esac],
       [gl_cv_header_working_fcntl_h=cross-compiling])])

  case $gl_cv_header_working_fcntl_h in #(
  *O_NOATIME* | no | cross-compiling) ac_val=0;; #(
  *) ac_val=1;;
  esac
  AC_DEFINE_UNQUOTED([HAVE_WORKING_O_NOATIME], [$ac_val],
    [Define to 1 if O_NOATIME works.])

  case $gl_cv_header_working_fcntl_h in #(
  *O_NOFOLLOW* | no | cross-compiling) ac_val=0;; #(
  *) ac_val=1;;
  esac
  AC_DEFINE_UNQUOTED([HAVE_WORKING_O_NOFOLLOW], [$ac_val],
    [Define to 1 if O_NOFOLLOW works.])
])
