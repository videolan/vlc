include (CheckTypeSize)

MACRO(vlc_check_type type header)
    # Construct the HAVE_ var
    string(REGEX REPLACE "[ -./:;,?=+\\]" "_" type_escaped ${type})
    string(TOUPPER ${type_escaped} type_escaped)
    set(CMAKE_EXTRA_INCLUDE_FILES ${header})
    check_type_size( ${type} HAVE_${type_escaped} )
    set(CMAKE_EXTRA_INCLUDE_FILES)
ENDMACRO(vlc_check_type)
