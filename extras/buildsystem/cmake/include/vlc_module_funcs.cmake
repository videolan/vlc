include( ${CMAKE_SOURCE_DIR}/cmake/vlc_add_compile_flag.cmake )

MACRO(vlc_add_module module_name)
    if(ENABLE_VLC_MODULE_${module_name})
        add_library( ${module_name}_plugin MODULE ${ARGN} )
        if( NOT ${ENABLE_NO_SYMBOL_CHECK} )
            vlc_module_add_link_libraries( ${module_name} libvlc )
        endif( NOT ${ENABLE_NO_SYMBOL_CHECK} )
        vlc_get_module_compile_flags(compile_flags ${module_name})
        set_target_properties( ${module_name}_plugin PROPERTIES COMPILE_FLAGS
                 "${compile_flags}" )
        set_target_properties( ${module_name}_plugin PROPERTIES LINK_FLAGS "${VLC_${module_name}_LINK_FLAGS}" )
        if (VLC_${module_name}_LINK_LIBRARIES)
            target_link_libraries( ${module_name}_plugin ${VLC_${module_name}_LINK_LIBRARIES})
        endif (VLC_${module_name}_LINK_LIBRARIES)
        install_targets(/modules ${module_name}_plugin)
    endif(ENABLE_VLC_MODULE_${module_name})
ENDMACRO(vlc_add_module)

MACRO(vlc_get_module_compile_flags var module_name)
    set(${var} "-D__PLUGIN__ -DMODULE_STRING=\\\"${module_name}\\\" -DMODULE_NAME_IS_${module_name} -I${CMAKE_CURRENT_SOURCE_DIR} ${VLC_${module_name}_COMPILE_FLAG}")
ENDMACRO(vlc_get_module_compile_flags)

MACRO(vlc_register_modules module_state)
    foreach( module_name ${ARGN} )
        set( STATE module_state)
        if(STATE)
            set(VLC_ENABLED_MODULES_LIST ${VLC_ENABLED_MODULES_LIST} ${module_name}_plugin)
        else(STATE)
        endif(STATE)
        OPTION( ENABLE_VLC_MODULE_${module_name} "Enable the ${module_name} module" ${module_state} )
    endforeach( module_name )
ENDMACRO(vlc_register_modules)

MACRO(vlc_enable_modules module_names)
    vlc_register_modules( ON ${ARGV} )
ENDMACRO(vlc_enable_modules)

MACRO(vlc_disable_modules module_names)
    vlc_register_modules( OFF ${ARGV} )
ENDMACRO(vlc_disable_modules)

MACRO(vlc_set_module_properties module_name)
    set_target_properties(${module_name}_plugin ${ARGN})
ENDMACRO(vlc_set_module_properties)

MACRO(vlc_module_add_link_flags module_name)
    set(VLC_${module_name}_LINK_FLAGS ${VLC_${module_name}_LINK_FLAGS} ${ARGN})
ENDMACRO(vlc_module_add_link_flags)

MACRO(vlc_module_add_link_libraries module_name)
    set(VLC_${module_name}_LINK_LIBRARIES ${VLC_${module_name}_LINK_LIBRARIES} ${ARGN})
ENDMACRO(vlc_module_add_link_libraries)

MACRO(vlc_add_module_compile_flag module_name)
    set(VLC_${module_name}_COMPILE_FLAG ${VLC_${module_name}_COMPILE_FLAG} ${ARGN})
ENDMACRO(vlc_add_module_compile_flag)

