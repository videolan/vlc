# - Try to find automoc4
# Once done this will define
#
#  AUTOMOC4_FOUND - automoc4 has been found
#  AUTOMOC4_EXECUTABLE - the automoc4 tool
#  AUTOMOC4_VERSION - the full version of automoc4
#  AUTOMOC4_VERSION_MAJOR, AUTOMOC4_VERSION_MINOR, AUTOMOC4_VERSION_PATCH - AUTOMOC4_VERSION
#                     broken into its components
#
# It also adds the following macros
#  AUTOMOC4(<target> <SRCS_VAR>)
#    Use this to run automoc4 on all files contained in the list <SRCS_VAR>.
#
#  AUTOMOC4_MOC_HEADERS(<target> header1.h header2.h ...)
#    Use this to add more header files to be processed with automoc4.
#
#  AUTOMOC4_ADD_EXECUTABLE(<target_NAME> src1 src2 ...)
#    This macro does the same as ADD_EXECUTABLE, but additionally
#    adds automoc4 handling for all source files.
#
# AUTOMOC4_ADD_LIBRARY(<target_NAME> src1 src2 ...)
#    This macro does the same as ADD_LIBRARY, but additionally
#    adds automoc4 handling for all source files.

# Internal helper macro, may change or be removed anytime:
# _ADD_AUTOMOC4_TARGET(<target_NAME> <SRCS_VAR>)
#
# Since version 0.9.88:
# The following two macros are only to be used for KDE4 projects
# and do something which makes sure automoc4 works for KDE. Don't
# use them anywhere else.
# _AUTOMOC4_KDE4_PRE_TARGET_HANDLING(<target_NAME> <SRCS_VAR>)
# _AUTOMOC4_KDE4_POST_TARGET_HANDLING(<target_NAME>)


# Copyright (c) 2008-2009, Alexander Neundorf, <neundorf@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.


# check if we are inside KDESupport and automoc is enabled
if("${KDESupport_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
   # when building this project as part of kdesupport
   set(AUTOMOC4_CONFIG_FILE "${KDESupport_SOURCE_DIR}/automoc/Automoc4Config.cmake")
else("${KDESupport_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
   # when building this project outside kdesupport

   # CMAKE_[SYSTEM_]PREFIX_PATH exists starting with cmake 2.6.0
   file(TO_CMAKE_PATH "$ENV{CMAKE_PREFIX_PATH}" _env_CMAKE_PREFIX_PATH)
   file(TO_CMAKE_PATH "$ENV{CMAKE_LIBRARY_PATH}" _env_CMAKE_LIBRARY_PATH)

   find_file(AUTOMOC4_CONFIG_FILE NAMES Automoc4Config.cmake
             PATH_SUFFIXES automoc4 lib/automoc4 lib64/automoc4
             PATHS ${_env_CMAKE_PREFIX_PATH} ${CMAKE_PREFIX_PATH} ${CMAKE_SYSTEM_PREFIX_PATH}
                   ${_env_CMAKE_LIBRARY_PATH} ${CMAKE_LIBRARY_PATH} ${CMAKE_SYSTEM_LIBRARY_PATH}
                   ${CMAKE_INSTALL_PREFIX}
             NO_DEFAULT_PATH )
endif("${KDESupport_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")


if(AUTOMOC4_CONFIG_FILE)
   include(${AUTOMOC4_CONFIG_FILE})
   set(AUTOMOC4_FOUND TRUE)
else(AUTOMOC4_CONFIG_FILE)
   set(AUTOMOC4_FOUND FALSE)
endif(AUTOMOC4_CONFIG_FILE)

if (AUTOMOC4_FOUND)
   if (NOT Automoc4_FIND_QUIETLY)
      message(STATUS "Found Automoc4: ${AUTOMOC4_EXECUTABLE}")
   endif (NOT Automoc4_FIND_QUIETLY)
else (AUTOMOC4_FOUND)
   if (Automoc4_FIND_REQUIRED)
      message(FATAL_ERROR "Did not find automoc4 (part of kdesupport).")
   else (Automoc4_FIND_REQUIRED)
      if (NOT Automoc4_FIND_QUIETLY)
         message(STATUS "Did not find automoc4 (part of kdesupport).")
      endif (NOT Automoc4_FIND_QUIETLY)
   endif (Automoc4_FIND_REQUIRED)
endif (AUTOMOC4_FOUND)
