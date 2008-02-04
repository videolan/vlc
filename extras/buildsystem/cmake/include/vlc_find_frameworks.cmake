include(CMakeFindFrameworks)

MACRO(vlc_find_frameworks frameworks)
    foreach(framework ${ARGV})
        cmake_find_frameworks(${framework})
    endforeach(framework)
ENDMACRO(vlc_find_frameworks)