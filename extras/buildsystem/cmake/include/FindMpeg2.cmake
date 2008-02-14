# - Find library containing Mpeg2()
# The following variables are set if Mpeg2 is found. If Mpeg2 is not
# found, Mpeg2_FOUND is set to false.
#  Mpeg2_FOUND     - System has Mpeg2.
#  Mpeg2_LIBRARIES - Link these to use Mpeg2.
#  Mpeg2_CFLAGS - Link these to use Mpeg2.


if (NOT Mpeg2_SEARCHED)
    include(CheckLibraryExists)

    set(Mpeg2_SEARCHED TRUE CACHE INTERNAL "")
    set(Mpeg2_FOUND FALSE CACHE INTERNAL "")

    pkg_check_modules(Mpeg2 mpeg2)

    if (NOT Mpeg2_FOUND)
        find_library(Mpeg2_LIBRARY NAMES mpeg2)
        if (Mpeg2_LIBRARY)
              set(Mpeg2_LIBRARIES "${Mpeg2_LIBRARY}" CACHE INTERNAL "")
              set(Mpeg2_FOUND TRUE CACHE INTERNAL "")
        endif (Mpeg2_LIBRARY)
    endif (NOT Mpeg2_FOUND)

    if (Mpeg2_FOUND)
      if (NOT Mpeg2_FIND_QUIETLY)
        message(STATUS "Found Mpeg2 in: ${Mpeg2_LIBRARIES}")
      endif (NOT Mpeg2_FIND_QUIETLY)
    else (Mpeg2_FOUND)
      if (Mpeg2_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find the library containing Mpeg2")
      endif (Mpeg2_FIND_REQUIRED)
    endif (Mpeg2_FOUND)

    mark_as_advanced(Mpeg2_LIBRARIES)
endif(NOT Mpeg2_SEARCHED)
