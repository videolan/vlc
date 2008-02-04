include (CheckIncludeFiles)

MACRO(vlc_check_include_files files)
    foreach(filepath ${ARGV})
        # Construct the HAVE_ var
        string(REGEX REPLACE "[-./:;,?=+\\]" "_" filepath_escaped ${filepath})
        string(TOUPPER ${filepath_escaped} filepath_escaped)
        check_include_files( ${filepath} HAVE_${filepath_escaped} )
    endforeach(filepath)
ENDMACRO(vlc_check_include_files)
