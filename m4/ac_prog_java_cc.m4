dnl @synopsis AC_PROG_JAVA_CC
dnl
dnl Finds the appropriate java compiler on your path. By preference the
dnl java compiler is gcj, then jikes then javac.
dnl
dnl The macro can take one argument specifying a space separated list
dnl of java compiler names.
dnl
dnl For example:
dnl
dnl   AC_PROG_JAVA_CC(javac, gcj)
dnl
dnl The macro also sets the compiler options variable: JAVA_CC_OPTS to
dnl something sensible:
dnl
dnl  - for GCJ it sets it to: @GCJ_OPTS@
dnl    (if GCJ_OPTS is not yet defined then it is set to "-C")
dnl
dnl  - no other compiler has applicable options yet
dnl
dnl Here's an example configure.in:
dnl
dnl   AC_INIT(Makefile.in)
dnl   AC_PROG_JAVA_CC()
dnl   AC_OUTPUT(Makefile)
dnl   dnl End.
dnl
dnl And here's the start of the Makefile.in:
dnl
dnl   PROJECT_ROOT      := @srcdir@
dnl   # Tool definitions.
dnl   JAVAC             := @JAVA_CC@
dnl   JAVAC_OPTS        := @JAVA_CC_OPTS@
dnl   JAR_TOOL          := @jar_tool@
dnl
dnl @category Java
dnl @author Nic Ferrier <nferrier@tapsellferrier.co.uk>
dnl @version 2002-03-04
dnl @license GPLWithACException

# AC_PROG_JAVA_CC([COMPILER ...])
# --------------------------
# COMPILER ... is a space separated list of java compilers to search for.
# This just gives the user an opportunity to specify an alternative
# search list for the java compiler.
AC_DEFUN([AC_PROG_JAVA_CC],
[AC_ARG_VAR([JAVA_CC],                [java compiler command])dnl
AC_ARG_VAR([JAVA_CC_FLAGS],           [java compiler flags])dnl
m4_ifval([$1],
      [AC_CHECK_TOOLS(JAVA_CC, [$1])],
[AC_CHECK_TOOL(JAVA_CC, gcj)
if test -z "$JAVA_CC"; then
  AC_CHECK_TOOL(JAVA_CC, javac)
fi
if test -z "$JAVA_CC"; then
  AC_CHECK_TOOL(JAVA_CC, jikes)
fi
])

if test "$JAVA_CC" = "gcj"; then
   if test "$GCJ_OPTS" = ""; then
      AC_SUBST(GCJ_OPTS,-C)
   fi
   AC_SUBST(JAVA_CC_OPTS, @GCJ_OPTS@,
        [Define the compilation options for GCJ])
fi
test -z "$JAVA_CC" && AC_MSG_ERROR([no acceptable java compiler found in \$PATH])
])# AC_PROG_JAVA_CC
