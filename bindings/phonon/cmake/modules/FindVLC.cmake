# - Try to find VLC library
# Once done this will define
#
#  VLC_FOUND - system has VLC
#  VLC_INCLUDE_DIR - The VLC include directory
#  VLC_LIBRARIES - The libraries needed to use VLC
#  VLC_DEFINITIONS - Compiler switches required for using VLC
#
# Copyright (C) 2008, Tanguy Krotoff <tkrotoff@gmail.com>
# Copyright (C) 2008, Lukas Durfina <lukas.durfina@gmail.com>
# Copyright (c) 2009, Fathi Boudra <fboudra@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#

if(VLC_INCLUDE_DIR AND VLC_LIBRARIES)
   # in cache already
   set(VLC_FIND_QUIETLY TRUE)
endif(VLC_INCLUDE_DIR AND VLC_LIBRARIES)

# use pkg-config to get the directories and then use these values
# in the FIND_PATH() and FIND_LIBRARY() calls
if(NOT WIN32)
  find_package(PkgConfig)
  pkg_check_modules(VLC libvlc>=1.0.0)
  set(VLC_DEFINITIONS ${VLC_CFLAGS})
  set(VLC_LIBRARIES ${VLC_LDFLAGS})
endif(NOT WIN32)

# TODO add argument support to pass version on find_package
include(MacroEnsureVersion)
macro_ensure_version(1.0.0 ${VLC_VERSION} VLC_VERSION_OK)
if(VLC_VERSION_OK)
  set(VLC_FOUND TRUE)
  message(STATUS "VLC library found")
else(VLC_VERSION_OK)
  set(VLC_FOUND FALSE)
  message(FATAL_ERROR "VLC library not found")
endif(VLC_VERSION_OK)

find_path(VLC_INCLUDE_DIR
          NAMES vlc.h
          PATHS ${VLC_INCLUDE_DIRS}
          PATH_SUFFIXES vlc)

find_library(VLC_LIBRARIES
             NAMES vlc
             PATHS ${VLC_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(VLC DEFAULT_MSG VLC_INCLUDE_DIR VLC_LIBRARIES)

# show the VLC_INCLUDE_DIR and VLC_LIBRARIES variables only in the advanced view
mark_as_advanced(VLC_INCLUDE_DIR VLC_LIBRARIES)
