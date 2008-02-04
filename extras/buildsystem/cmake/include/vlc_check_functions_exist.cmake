include (CheckFunctionExists)

MACRO(vlc_check_functions_exist functions)
    foreach(function ${ARGV})
        # Construct the HAVE_ var
        string(REGEX REPLACE "[-./:;,?=+\\]" "_" function_escaped ${function})
        string(TOUPPER ${function_escaped} function_escaped)
        check_function_exists( ${function} HAVE_${function_escaped} )
    endforeach(function)
ENDMACRO(vlc_check_functions_exist)
