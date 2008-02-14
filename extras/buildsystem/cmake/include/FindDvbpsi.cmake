# - Find library containing Dvbpsi()
# The following variables are set if Dvbpsi is found. If Dvbpsi is not
# found, Dvbpsi_FOUND is set to false.
#  Dvbpsi_FOUND     - System has Dvbpsi.
#  Dvbpsi_LIBRARIES - Link these to use Dvbpsi.
#  Dvbpsi_CFLAGS - Link these to use Dvbpsi.


if (NOT Dvbpsi_SEARCHED)
    include(CheckLibraryExists)

    set(Dvbpsi_SEARCHED TRUE CACHE INTERNAL "")
    set(Dvbpsi_FOUND FALSE CACHE INTERNAL "")

    pkg_check_modules(Dvbpsi dvbpsi)

    if (NOT Dvbpsi_FOUND)
        find_library(Dvbpsi_LIBRARY NAMES dvbpsi)
        if (Dvbpsi_LIBRARY)
              set(Dvbpsi_LIBRARIES "${Dvbpsi_LIBRARY}" CACHE INTERNAL "")
              set(Dvbpsi_FOUND TRUE CACHE INTERNAL "")
        endif (Dvbpsi_LIBRARY)
    endif (NOT Dvbpsi_FOUND)

    if (Dvbpsi_FOUND)
      if (NOT Dvbpsi_FIND_QUIETLY)
        message(STATUS "Found Dvbpsi in: ${Dvbpsi_LIBRARIES}")
      endif (NOT Dvbpsi_FIND_QUIETLY)
    else (Dvbpsi_FOUND)
      if (Dvbpsi_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find the library containing Dvbpsi")
      endif (Dvbpsi_FIND_REQUIRED)
    endif (Dvbpsi_FOUND)

    mark_as_advanced(Dvbpsi_LIBRARIES)
endif(NOT Dvbpsi_SEARCHED)
