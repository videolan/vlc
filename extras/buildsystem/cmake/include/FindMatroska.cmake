# - Find library containing Matroska()
# The following variables are set if Matroska is found. If Matroska is not
# found, Matroska_FOUND is set to false.
#  Matroska_FOUND     - System has Matroska.
#  Matroska_LIBRARIES - Link these to use Matroska.
#  Matroska_CFLAGS - Link these to use Matroska.


if (NOT Matroska_SEARCHED)
    include(CheckLibraryExists)

    set(Matroska_SEARCHED TRUE CACHE INTERNAL "")
    set(Matroska_FOUND FALSE CACHE INTERNAL "")

    pkg_check_modules(matroska libmatroska)

    if (NOT Matroska_FOUND)
        set(Matroska_LIBRARIES "")
        find_library( matroska_LIBRARY matroska )
        if (matroska_LIBRARY)
              set(Matroska_LIBRARIES "${matroska_LIBRARY}")
              set(Matroska_FOUND TRUE CACHE INTERNAL "")
        endif (matroska_LIBRARY)
        foreach (library ebml ebml_pic)
            find_library( ${library}_LIBRARY ${library} )
            if (${library}_LIBRARY)
              set(Matroska_LIBRARIES "${library};${Matroska_LIBRARIES}")
            endif (${library}_LIBRARY)
        endforeach (library)
        set(Matroska_LIBRARIES "${Matroska_LIBRARIES}" CACHE INTERNAL STRING)
    endif (NOT Matroska_FOUND)

    if (Matroska_FOUND)
      if (NOT Matroska_FIND_QUIETLY)
        message(STATUS "Found Matroska in: ${Matroska_LIBRARIES}")
      endif (NOT Matroska_FIND_QUIETLY)
    else (Matroska_FOUND)
      if (Matroska_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find the library containing Matroska")
      endif (Matroska_FIND_REQUIRED)
    endif (Matroska_FOUND)

    mark_as_advanced(Matroska_LIBRARIES)
endif(NOT Matroska_SEARCHED)
