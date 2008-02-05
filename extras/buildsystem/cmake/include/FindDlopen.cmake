# From licq www.licq.org (GPL)
# - Find library containing dlopen()
# The following variables are set if dlopen is found. If dlopen is not
# found, Dlopen_FOUND is set to false.
#  Dlopen_FOUND     - System has dlopen.
#  Dlopen_LIBRARIES - Link these to use dlopen.
#
# Copyright (c) 2007 Erik Johansson <erik@ejohansson.se>
# Redistribution and use is allowed according to the terms of the BSD license.


if (NOT Dlopen_SEARCHED)
    include(CheckLibraryExists)

    set(Dlopen_SEARCHED TRUE CACHE INTERNAL "")
    set(Dlopen_FOUND FALSE CACHE INTERNAL "")

    foreach (library c c_r dl)
      if (NOT Dlopen_FOUND)
        check_library_exists(${library} dlopen "" Dlopen_IN_${library})

        if (Dlopen_IN_${library})
          set(Dlopen_LIBRARIES ${library} CACHE STRING "Library containing dlopen")
          set(Dlopen_FOUND TRUE CACHE INTERNAL "")
        endif (Dlopen_IN_${library})

      endif (NOT Dlopen_FOUND)
    endforeach (library)

    if (Dlopen_FOUND)
      if (NOT Dlopen_FIND_QUIETLY)
        message(STATUS "Found dlopen in: ${Dlopen_LIBRARIES}")
      endif (NOT Dlopen_FIND_QUIETLY)
    else (Dlopen_FOUND)
      if (Dlopen_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find the library containing dlopen")
      endif (Dlopen_FIND_REQUIRED)
    endif (Dlopen_FOUND)

    mark_as_advanced(Dlopen_LIBRARIES)
endif(NOT Dlopen_SEARCHED)
