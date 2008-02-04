include( ${CMAKE_SOURCE_DIR}/cmake/vlc_add_compile_flag.cmake )

MACRO(vlc_add_module module_name)
    if(ENABLE_VLC_MODULE_${module_name})
        add_library( vlc_${module_name} MODULE ${ARGN} )
        set_target_properties( vlc_${module_name} PROPERTIES COMPILE_FLAGS "-D__PLUGIN__ -DMODULE_NAME=${module_name} -DMODULE_NAME_IS_${module_name} -I${CMAKE_CURRENT_SOURCE_DIR}" )
        if (VLC_${module_name}_LINK_LIBRARIES)
            target_link_libraries( vlc_${module_name} libvlc ${VLC_${module_name}_LINK_LIBRARIES})
        else (VLC_${module_name}_LINK_LIBRARIES)
            target_link_libraries( vlc_${module_name} libvlc )
        endif (VLC_${module_name}_LINK_LIBRARIES)
    endif(ENABLE_VLC_MODULE_${module_name})
ENDMACRO(vlc_add_module)

MACRO(vlc_register_modules state)
    foreach( module_name ${ARGN} )
        OPTION( ENABLE_VLC_MODULE_${module_name} "Enable the ${module_name} module" ${state} )
    endforeach( module_name )
ENDMACRO(vlc_register_modules)

MACRO(vlc_enable_modules module_names)
    vlc_register_modules( ON ${ARGV} )
ENDMACRO(vlc_enable_modules)

MACRO(vlc_disable_modules module_names)
    vlc_register_modules( OFF ${ARGV} )
ENDMACRO(vlc_disable_modules)

MACRO(vlc_set_module_properties module_name)
    set_target_properties(vlc_${module_name} ${ARGN})
ENDMACRO(vlc_set_module_properties)

MACRO(vlc_set_module_properties module_name)
    set_target_properties(vlc_${module_name} ${ARGN})
ENDMACRO(vlc_set_module_properties)

MACRO(vlc_module_add_link_libraries module_name)
    set(VLC_${module_name}_LINK_LIBRARIES ${VLC_${module_name}_LINK_LIBRARIES} ${ARGN})
ENDMACRO(vlc_module_add_link_libraries)

