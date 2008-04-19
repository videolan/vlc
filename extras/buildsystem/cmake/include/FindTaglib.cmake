# - Find library containing Taglib()
# The following variables are set if Taglib is found. If Taglib is not
# found, Taglib_FOUND is set to false.
#  Taglib_FOUND     - System has Taglib.
#  Taglib_LIBRARIES - Link these to use Taglib.
#  Taglib_CFLAGS - Link these to use Taglib.


if (NOT Taglib_SEARCHED)
    include(CheckLibraryExists)

    set(Taglib_SEARCHED TRUE CACHE INTERNAL "")
    set(Taglib_FOUND FALSE CACHE INTERNAL "")

    pkg_check_modules(Taglib taglib)

    if (Taglib_FOUND)
      if (NOT Taglib_FIND_QUIETLY)
        message(STATUS "Found Taglib in: ${Taglib_LIBRARIES}")
      endif (NOT Taglib_FIND_QUIETLY)
    else (Taglib_FOUND)
      if (Taglib_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find the library containing Taglib")
      endif (Taglib_FIND_REQUIRED)
    endif (Taglib_FOUND)

    mark_as_advanced(Taglib_LIBRARIES)
endif(NOT Taglib_SEARCHED)
