# - Find library containing Lua()
# The following variables are set if Lua is found. If Lua is not
# found, Lua_FOUND is set to false.
#  Lua_FOUND     - System has Lua.
#  Lua_LIBRARIES - Link these to use Lua.
#  Lua_CFLAGS - Link these to use Lua.


if (NOT Lua_SEARCHED)
    include(CheckLibraryExists)

    set(Lua_SEARCHED TRUE CACHE INTERNAL "")
    set(Lua_FOUND FALSE CACHE INTERNAL "")

    pkg_check_modules(Lua lua>=5.1)
    if (NOT Lua_FOUND)
        pkg_check_modules(Lua lua5.1)
    endif (NOT Lua_FOUND)

    if (NOT Lua_FOUND)
        find_library( Lua_LIBRARY NAMES lua5.1 lua51 lua)
        if (Lua_LIBRARY)
              set(Lua_LIBRARIES "${Lua_LIBRARY}" CACHE INTERNAL "")
              set(Lua_FOUND TRUE CACHE INTERNAL "")
        endif (Lua_LIBRARY)
    endif (NOT Lua_FOUND)

    if (Lua_FOUND)
      if (NOT Lua_FIND_QUIETLY)
        message(STATUS "Found Lua in: ${Lua_LIBRARIES}")
      endif (NOT Lua_FIND_QUIETLY)
    else (Lua_FOUND)
      if (Lua_FIND_REQUIRED)
        message(FATAL_ERROR "Could not find the library containing Lua")
      endif (Lua_FIND_REQUIRED)
    endif (Lua_FOUND)

    mark_as_advanced(Lua_LIBRARIES)
endif(NOT Lua_SEARCHED)
