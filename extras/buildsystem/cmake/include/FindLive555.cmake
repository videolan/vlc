# - Find library containing Live555()
# The following variables are set if Live555 is found. If Live555 is not
# found, Live555_FOUND is set to false.
#  Live555_FOUND     - System has Live555.
#  Live555_LIBRARIES - Link these to use Live555.
#  Live555_CFLAGS - Link these to use Live555.


if (NOT Live555_SEARCHED)
    include(CheckLibraryExists)

    set(Live555_SEARCHED TRUE CACHE INTERNAL "")
    set(Live555_FOUND FALSE CACHE INTERNAL "")

    pkg_check_modules(Live555 libLive555)

    if (NOT Live555_FOUND)
        set(Live555_LIBRARIES "")
        foreach (library livemedia livemedia_pic)
            find_library( ${library}_LIBRARY ${library} )
            if (${library}_LIBRARY)
              set(Live555_LIBRARIES "${library};${Live555_LIBRARIES}")
              set(Live555_FOUND TRUE CACHE INTERNAL "")
            endif (${library}_LIBRARY)
        endforeach (library)
        foreach (library groupsock_pic groupsock BasicUsageEnvironment_pic BasicUsageEnvironment UsageEnvironment_pic UsageEnvironment)
            find_library( ${library}_LIBRARY ${library} )
            if (${library}_LIBRARY)
              set(Live555_LIBRARIES "${library};${Live555_LIBRARIES}")
            endif (${library}_LIBRARY)
        endforeach (library)
        set(Live555_LIBRARIES "${Live555_LIBRARIES}" CACHE INTERNAL STRING)
    endif (NOT Live555_FOUND)

    if (Live555_FOUND)
      if (NOT Live555_FIND_QUIETLY)
        message(STATUS "Found Live555 in: ${Live555_LIBRARIES}")
      endif (NOT Live555_FIND_QUIETLY)
    else (Live555_FOUND)
      if (Live555_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find the library containing Live555")
      endif (Live555_FIND_REQUIRED)
    endif (Live555_FOUND)

    mark_as_advanced(Live555_LIBRARIES)
endif(NOT Live555_SEARCHED)
