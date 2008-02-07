# - Find library containing FFmpeg()
# The following variables are set if FFmpeg is found. If FFmpeg is not
# found, FFmpeg_FOUND is set to false.
#  FFmpeg_FOUND     - System has FFmpeg.
#  FFmpeg_LIBRARIES - Link these to use FFmpeg.
#  FFmpeg_CFLAGS - Link these to use FFmpeg.


if (NOT FFmpeg_SEARCHED)
    include(CheckLibraryExists)

    set(FFmpeg_SEARCHED TRUE CACHE INTERNAL "")
    set(FFmpeg_FOUND FALSE CACHE INTERNAL "")

    pkg_check_modules(FFmpeg libffmpeg)

    if (NOT FFmpeg_FOUND)
        set(FFmpeg_LIBRARIES "")
        foreach (library ffmpeg avcodec avformat avutil postproc)
            find_library( ${library}_LIBRARY ${library} )
            set( ${library}_LIBRARY_FOUND NOT ${${library}_LIBRARY} )
            if (${library}_LIBRARY_FOUND)
              set(FFmpeg_LIBRARIES "${library};${FFmpeg_LIBRARIES}")
              set(FFmpeg_FOUND TRUE CACHE INTERNAL "")
            endif (${library}_LIBRARY_FOUND)
        endforeach (library)
        foreach (library a52 faac lame z png mp3lame twolame)
            find_library( ${library}_LIBRARY ${library} )
            set( ${library}_LIBRARY_FOUND NOT ${${library}_LIBRARY} )
            if (${library}_LIBRARY_FOUND)
              set(FFmpeg_LIBRARIES "${library};${FFmpeg_LIBRARIES}")
            endif (${library}_LIBRARY_FOUND)
        endforeach (library)
        set(FFmpeg_LIBRARIES "${FFmpeg_LIBRARIES}" CACHE INTERNAL STRING)
    endif (NOT FFmpeg_FOUND)

    if (FFmpeg_FOUND)
      if (NOT FFmpeg_FIND_QUIETLY)
        message(STATUS "Found FFmpeg in: ${FFmpeg_LIBRARIES}")
      endif (NOT FFmpeg_FIND_QUIETLY)
    else (FFmpeg_FOUND)
      if (FFmpeg_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find the library containing FFmpeg")
      endif (FFmpeg_FIND_REQUIRED)
    endif (FFmpeg_FOUND)

    mark_as_advanced(FFmpeg_LIBRARIES)
endif(NOT FFmpeg_SEARCHED)
