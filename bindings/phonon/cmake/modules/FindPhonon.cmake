# Find libphonon
# Once done this will define
#
#  PHONON_FOUND    - system has Phonon Library
#  PHONON_INCLUDES - the Phonon include directory
#  PHONON_LIBS     - link these to use Phonon
#  PHONON_VERSION  - the version of the Phonon Library

# Copyright (c) 2008, Matthias Kretz <kretz@kde.org>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

macro(_phonon_find_version)
   set(_phonon_namespace_header_file "${PHONON_INCLUDE_DIR}/phonon/phononnamespace.h")
   if (APPLE AND EXISTS "${PHONON_INCLUDE_DIR}/Headers/phononnamespace.h")
      set(_phonon_namespace_header_file "${PHONON_INCLUDE_DIR}/Headers/phononnamespace.h")
   endif (APPLE AND EXISTS "${PHONON_INCLUDE_DIR}/Headers/phononnamespace.h")
   file(READ ${_phonon_namespace_header_file} _phonon_header LIMIT 5000 OFFSET 1000)
   string(REGEX MATCH "define PHONON_VERSION_STR \"(4\\.[0-9]+\\.[0-9a-z]+)\"" _phonon_version_match "${_phonon_header}")
   set(PHONON_VERSION "${CMAKE_MATCH_1}")
   message(STATUS "Phonon Version: ${PHONON_VERSION}")
endmacro(_phonon_find_version)

if(PHONON_FOUND)
   # Already found, nothing more to do except figuring out the version
   _phonon_find_version()
else(PHONON_FOUND)
   if(PHONON_INCLUDE_DIR AND PHONON_LIBRARY)
      set(PHONON_FIND_QUIETLY TRUE)
   endif(PHONON_INCLUDE_DIR AND PHONON_LIBRARY)

   # As discussed on kde-buildsystem: first look at CMAKE_PREFIX_PATH, then at the suggested PATHS (kde4 install dir)
   find_library(PHONON_LIBRARY NAMES phonon PATHS ${KDE4_LIB_INSTALL_DIR} ${QT_LIBRARY_DIR} NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
   # then at the default system locations (CMAKE_SYSTEM_PREFIX_PATH, i.e. /usr etc.)
   find_library(PHONON_LIBRARY NAMES phonon)

   find_path(PHONON_INCLUDE_DIR NAMES phonon/phonon_export.h PATHS ${KDE4_INCLUDE_INSTALL_DIR} ${QT_INCLUDE_DIR} ${INCLUDE_INSTALL_DIR} ${QT_LIBRARY_DIR} NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH)
   find_path(PHONON_INCLUDE_DIR NAMES phonon/phonon_export.h)

   if(PHONON_INCLUDE_DIR AND PHONON_LIBRARY)
      set(PHONON_LIBS ${phonon_LIB_DEPENDS} ${PHONON_LIBRARY})
      set(PHONON_INCLUDES ${PHONON_INCLUDE_DIR}/KDE ${PHONON_INCLUDE_DIR})
      set(PHONON_FOUND TRUE)
      _phonon_find_version()
   else(PHONON_INCLUDE_DIR AND PHONON_LIBRARY)
      set(PHONON_FOUND FALSE)
   endif(PHONON_INCLUDE_DIR AND PHONON_LIBRARY)

   if(PHONON_FOUND)
      if(NOT PHONON_FIND_QUIETLY)
         message(STATUS "Found Phonon: ${PHONON_LIBRARY}")
         message(STATUS "Found Phonon Includes: ${PHONON_INCLUDES}")
      endif(NOT PHONON_FIND_QUIETLY)
   else(PHONON_FOUND)
      if(Phonon_FIND_REQUIRED)
         if(NOT PHONON_INCLUDE_DIR)
            message(STATUS "Phonon includes NOT found!")
         endif(NOT PHONON_INCLUDE_DIR)
         if(NOT PHONON_LIBRARY)
            message(STATUS "Phonon library NOT found!")
         endif(NOT PHONON_LIBRARY)
         message(FATAL_ERROR "Phonon library or includes NOT found!")
      else(Phonon_FIND_REQUIRED)
         message(STATUS "Unable to find Phonon")
      endif(Phonon_FIND_REQUIRED)
   endif(PHONON_FOUND)


   mark_as_advanced(PHONON_INCLUDE_DIR PHONON_LIBRARY PHONON_INCLUDES)
endif(PHONON_FOUND)
