
# Phonon helper macros:
#
# macro (phonon_add_executable _target)
# macro (PHONON_ADD_UNIT_TEST _test_NAME)
# macro (PHONON_UPDATE_ICONCACHE)
# macro (PHONON_UPDATE_ICONCACHE)
# macro (_PHONON_ADD_ICON_INSTALL_RULE _install_SCRIPT _install_PATH _group _orig_NAME _install_NAME _l10n_SUBDIR)
# macro (PHONON_INSTALL_ICONS _defaultpath )

set(_global_add_executable_param)
if (Q_WS_MAC)
   set(_global_add_executable_param MACOSX_BUNDLE)
endif (Q_WS_MAC)
if (WIN32)
   # no WIN32 here - all executables are command line executables
   set(_global_add_executable_param)
endif (WIN32)

macro(phonon_add_executable _target)
   set(_srcs ${ARGN})
   automoc4_add_executable(${_target} ${_global_add_executable_param} ${_srcs})
endmacro(phonon_add_executable _target)

macro (PHONON_ADD_UNIT_TEST _test_NAME)
   set(_srcList ${ARGN})
   set(_nogui)
   list(GET ${_srcList} 0 first_PARAM)
   set(_add_executable_param ${_global_add_executable_param})
   if(${first_PARAM} STREQUAL "NOGUI")
      set(_nogui "NOGUI")
      set(_add_executable_param)
   endif(${first_PARAM} STREQUAL "NOGUI")

   if (NOT PHONON_BUILD_TESTS)
      set(_add_executable_param ${_add_executable_param} EXCLUDE_FROM_ALL)
   endif (NOT PHONON_BUILD_TESTS)

   automoc4_add_executable(${_test_NAME} ${_add_executable_param} ${_srcList})

   if(NOT PHONON_TEST_OUTPUT)
      set(PHONON_TEST_OUTPUT plaintext)
   endif(NOT PHONON_TEST_OUTPUT)
   set(PHONON_TEST_OUTPUT ${PHONON_TEST_OUTPUT} CACHE STRING "The output to generate when running the QTest unit tests")

   set(using_qtest "")
   foreach(_filename ${_srcList})
      if(NOT using_qtest)
         if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_filename}")
            file(READ ${_filename} file_CONTENT)
            string(REGEX MATCH "QTEST_(KDE)?MAIN" using_qtest "${file_CONTENT}")
         endif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_filename}")
      endif(NOT using_qtest)
   endforeach(_filename)

   set(_executable ${EXECUTABLE_OUTPUT_PATH}/${_test_NAME})
   if (Q_WS_MAC AND NOT _nogui)
      set(_executable ${EXECUTABLE_OUTPUT_PATH}/${_test_NAME}.app/Contents/MacOS/${_test_NAME})
   else (Q_WS_MAC AND NOT _nogui)
      # Use .shell wrapper where available, to use uninstalled libs.
      #if (UNIX)
      #   set(_executable ${_executable}.shell)
      #endif (UNIX)
   endif (Q_WS_MAC AND NOT _nogui)

   if (using_qtest AND PHONON_TEST_OUTPUT STREQUAL "xml")
      add_test( ${_test_NAME} ${_executable} -xml -o ${_test_NAME}.tml)
   else (using_qtest AND PHONON_TEST_OUTPUT STREQUAL "xml")
      add_test( ${_test_NAME} ${_executable} )
   endif (using_qtest AND PHONON_TEST_OUTPUT STREQUAL "xml")

   if (NOT MSVC_IDE)   #not needed for the ide
      # if the tests are EXCLUDE_FROM_ALL, add a target "buildtests" to build all tests
      if (NOT PHONON_BUILD_TESTS)
         get_directory_property(_buildtestsAdded BUILDTESTS_ADDED)
         if(NOT _buildtestsAdded)
            add_custom_target(buildtests)
            set_directory_properties(PROPERTIES BUILDTESTS_ADDED TRUE)
         endif(NOT _buildtestsAdded)
         add_dependencies(buildtests ${_test_NAME})
      endif (NOT PHONON_BUILD_TESTS)
   endif (NOT MSVC_IDE)
endmacro (PHONON_ADD_UNIT_TEST)

macro (PHONON_UPDATE_ICONCACHE)
    # Update mtime of hicolor icon theme dir.
    # We don't always have touch command (e.g. on Windows), so instead create
    #  and delete a temporary file in the theme dir.
   install(CODE "
    set(DESTDIR_VALUE \"\$ENV{DESTDIR}\")
    if (NOT DESTDIR_VALUE)
        file(WRITE \"${ICON_INSTALL_DIR}/hicolor/temp.txt\" \"update\")
        file(REMOVE \"${ICON_INSTALL_DIR}/hicolor/temp.txt\")
    endif (NOT DESTDIR_VALUE)
    ")
endmacro (PHONON_UPDATE_ICONCACHE)

# a "map" of short type names to the directories
# unknown names should give empty results
# KDE 3 compatibility
set(_PHONON_ICON_GROUP_mime       "mimetypes")
set(_PHONON_ICON_GROUP_filesys    "places")
set(_PHONON_ICON_GROUP_device     "devices")
set(_PHONON_ICON_GROUP_app        "apps")
set(_PHONON_ICON_GROUP_action     "actions")
# KDE 4 / icon naming specification compatibility
set(_PHONON_ICON_GROUP_mimetypes  "mimetypes")
set(_PHONON_ICON_GROUP_places     "places")
set(_PHONON_ICON_GROUP_devices    "devices")
set(_PHONON_ICON_GROUP_apps       "apps")
set(_PHONON_ICON_GROUP_actions    "actions")
set(_PHONON_ICON_GROUP_categories "categories")
set(_PHONON_ICON_GROUP_status     "status")
set(_PHONON_ICON_GROUP_emblems    "emblems")
set(_PHONON_ICON_GROUP_emotes     "emotes")
set(_PHONON_ICON_GROUP_animations "animations")
set(_PHONON_ICON_GROUP_intl       "intl")

# a "map" of short theme names to the theme directory
set(_PHONON_ICON_THEME_ox "oxygen")
set(_PHONON_ICON_THEME_cr "crystalsvg")
set(_PHONON_ICON_THEME_lo "locolor")
set(_PHONON_ICON_THEME_hi "hicolor")

macro (_PHONON_ADD_ICON_INSTALL_RULE _install_SCRIPT _install_PATH _group _orig_NAME _install_NAME _l10n_SUBDIR)

   # if the string doesn't match the pattern, the result is the full string, so all three have the same content
   if (NOT ${_group} STREQUAL ${_install_NAME} )
      set(_icon_GROUP  ${_PHONON_ICON_GROUP_${_group}})
      if(NOT _icon_GROUP)
         set(_icon_GROUP "actions")
      endif(NOT _icon_GROUP)
#      message(STATUS "icon: ${_current_ICON} size: ${_size} group: ${_group} name: ${_name} l10n: ${_l10n_SUBDIR}")
      install(FILES ${_orig_NAME} DESTINATION ${_install_PATH}/${_icon_GROUP}/${_l10n_SUBDIR}/ RENAME ${_install_NAME} )
   endif (NOT ${_group} STREQUAL ${_install_NAME} )

endmacro (_PHONON_ADD_ICON_INSTALL_RULE)


macro (PHONON_INSTALL_ICONS _defaultpath )

   # the l10n-subdir if language given as second argument (localized icon)
   set(_lang ${ARGV1})
   if(_lang)
      set(_l10n_SUBDIR l10n/${_lang})
   else(_lang)
      set(_l10n_SUBDIR ".")
   endif(_lang)

   # first the png icons
   file(GLOB _icons *.png)
   foreach (_current_ICON ${_icons} )
      # since CMake 2.6 regex matches are stored in special variables CMAKE_MATCH_x, if it didn't match, they are empty
      string(REGEX MATCH "^.*/([a-zA-Z]+)([0-9]+)\\-([a-z]+)\\-(.+\\.png)$" _dummy  "${_current_ICON}")
      set(_type  "${CMAKE_MATCH_1}")
      set(_size  "${CMAKE_MATCH_2}")
      set(_group "${CMAKE_MATCH_3}")
      set(_name  "${CMAKE_MATCH_4}")

      set(_theme_GROUP ${_PHONON_ICON_THEME_${_type}})
      if( _theme_GROUP)
         _PHONON_ADD_ICON_INSTALL_RULE(${CMAKE_CURRENT_BINARY_DIR}/install_icons.cmake
                    ${_defaultpath}/${_theme_GROUP}/${_size}x${_size}
                    ${_group} ${_current_ICON} ${_name} ${_l10n_SUBDIR})
      endif( _theme_GROUP)
   endforeach (_current_ICON)

   # mng icons
   file(GLOB _icons *.mng)
   foreach (_current_ICON ${_icons} )
      # since CMake 2.6 regex matches are stored in special variables CMAKE_MATCH_x, if it didn't match, they are empty
      string(REGEX MATCH "^.*/([a-zA-Z]+)([0-9]+)\\-([a-z]+)\\-(.+\\.mng)$" _dummy  "${_current_ICON}")
      set(_type  "${CMAKE_MATCH_1}")
      set(_size  "${CMAKE_MATCH_2}")
      set(_group "${CMAKE_MATCH_3}")
      set(_name  "${CMAKE_MATCH_4}")

      set(_theme_GROUP ${_PHONON_ICON_THEME_${_type}})
      if( _theme_GROUP)
         _PHONON_ADD_ICON_INSTALL_RULE(${CMAKE_CURRENT_BINARY_DIR}/install_icons.cmake
                ${_defaultpath}/${_theme_GROUP}/${_size}x${_size}
                ${_group} ${_current_ICON} ${_name} ${_l10n_SUBDIR})
      endif( _theme_GROUP)
   endforeach (_current_ICON)

   # and now the svg icons
   file(GLOB _icons *.svgz)
   foreach (_current_ICON ${_icons} )
      # since CMake 2.6 regex matches are stored in special variables CMAKE_MATCH_x, if it didn't match, they are empty
      string(REGEX MATCH "^.*/([a-zA-Z]+)sc\\-([a-z]+)\\-(.+\\.svgz)$" _dummy "${_current_ICON}")
      set(_type  "${CMAKE_MATCH_1}")
      set(_group "${CMAKE_MATCH_2}")
      set(_name  "${CMAKE_MATCH_3}")

      set(_theme_GROUP ${_PHONON_ICON_THEME_${_type}})
      if( _theme_GROUP)
          _PHONON_ADD_ICON_INSTALL_RULE(${CMAKE_CURRENT_BINARY_DIR}/install_icons.cmake
                            ${_defaultpath}/${_theme_GROUP}/scalable
                            ${_group} ${_current_ICON} ${_name} ${_l10n_SUBDIR})
      endif( _theme_GROUP)
   endforeach (_current_ICON)

   phonon_update_iconcache()

endmacro (PHONON_INSTALL_ICONS)

