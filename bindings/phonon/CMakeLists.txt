project(Phonon)

cmake_minimum_required(VERSION 2.6.2 FATAL_ERROR)

# CMP0002: we have multiple targets with the same name for the unit tests
cmake_policy(SET CMP0002 OLD)

# enable unit tests
option(PHONON_BUILD_TESTS "Build the tests")
option(PHONON_BUILD_EXAMPLES "Build the examples")

if (PHONON_BUILD_TESTS)
    enable_testing()
endif (PHONON_BUILD_TESTS)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

include(PhononMacros)

include(MacroLogFeature)
include(MacroOptionalFindPackage)

set(QT_MIN_VERSION 4.4.0)
find_package(Qt4 REQUIRED)
if (NOT QT_QTDBUS_FOUND)
   message(STATUS "Warning: Phonon won't be compiled with DBus support.")
endif(NOT QT_QTDBUS_FOUND)

find_package(Automoc4 REQUIRED)
include (CheckCXXCompilerFlag)
include (MacroEnsureVersion)

find_package(Phonon REQUIRED)
find_package(VLC REQUIRED)
if (NOT AUTOMOC4_VERSION)
   set(AUTOMOC4_VERSION "0.9.83")
endif (NOT AUTOMOC4_VERSION)
macro_ensure_version("0.9.86" "${AUTOMOC4_VERSION}" _automoc4_version_ok)
if (NOT _automoc4_version_ok)
   message(FATAL_ERROR "Your version of automoc4 is too old. You have ${AUTOMOC4_VERSION}, you need at least 0.9.86")
endif (NOT _automoc4_version_ok)

if (CMAKE_COMPILER_IS_GNUCXX)
   set (KDE4_ENABLE_EXCEPTIONS -fexceptions)
   # Select flags.
   set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
   set(CMAKE_CXX_FLAGS_RELEASE        "-O2 -DNDEBUG -DQT_NO_DEBUG")
   set(CMAKE_CXX_FLAGS_DEBUG          "-g -O2 -fno-reorder-blocks -fno-schedule-insns -fno-inline")
   set(CMAKE_CXX_FLAGS_DEBUGFULL      "-g3 -fno-inline")
   set(CMAKE_CXX_FLAGS_PROFILE        "-g3 -fno-inline -ftest-coverage -fprofile-arcs")
   set(CMAKE_C_FLAGS_RELWITHDEBINFO   "-O2 -g")
   set(CMAKE_C_FLAGS_RELEASE          "-O2 -DNDEBUG -DQT_NO_DEBUG")
   set(CMAKE_C_FLAGS_DEBUG            "-g -O2 -fno-reorder-blocks -fno-schedule-insns -fno-inline")
   set(CMAKE_C_FLAGS_DEBUGFULL        "-g3 -fno-inline")
   set(CMAKE_C_FLAGS_PROFILE          "-g3 -fno-inline -ftest-coverage -fprofile-arcs")

   if (CMAKE_SYSTEM_NAME MATCHES Linux)
     set ( CMAKE_C_FLAGS     "${CMAKE_C_FLAGS} -Wno-long-long -std=iso9899:1990 -Wundef -Wcast-align -Werror-implicit-function-declaration -Wchar-subscripts -Wall -W -Wpointer-arith -Wwrite-strings -Wformat-security -Wmissing-format-attribute -fno-common")
     set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor -Wno-long-long -ansi -Wundef -Wcast-align -Wchar-subscripts -Wall -W -Wpointer-arith -Wformat-security -fno-exceptions -fno-check-new -fno-common")
     add_definitions (-D_BSD_SOURCE)
   endif (CMAKE_SYSTEM_NAME MATCHES Linux)

   # gcc under Windows
   if (MINGW)
      set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-all-symbols -Wl,--disable-auto-import")
      set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--export-all-symbols -Wl,--disable-auto-import")

      # we always link against the release version of QT with mingw
      # (even for debug builds). So we need to define QT_NO_DEBUG
      # or else QPluginLoader rejects plugins because it thinks
      # they're built against the wrong QT.
      add_definitions(-DQT_NO_DEBUG)
   endif (MINGW)

   check_cxx_compiler_flag(-fPIE HAVE_FPIE_SUPPORT)
   if(KDE4_ENABLE_FPIE)
       if(HAVE_FPIE_SUPPORT)
        set (KDE4_CXX_FPIE_FLAGS "-fPIE")
        set (KDE4_PIE_LDFLAGS "-pie")
       else(HAVE_FPIE_SUPPORT)
        message(STATUS "Your compiler doesn't support PIE flag")
       endif(HAVE_FPIE_SUPPORT)
   endif(KDE4_ENABLE_FPIE)

   check_cxx_compiler_flag(-Woverloaded-virtual __KDE_HAVE_W_OVERLOADED_VIRTUAL)
   if(__KDE_HAVE_W_OVERLOADED_VIRTUAL)
       set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Woverloaded-virtual")
   endif(__KDE_HAVE_W_OVERLOADED_VIRTUAL)

   # visibility support
   check_cxx_compiler_flag(-fvisibility=hidden __KDE_HAVE_GCC_VISIBILITY)
   set( __KDE_HAVE_GCC_VISIBILITY ${__KDE_HAVE_GCC_VISIBILITY} CACHE BOOL "GCC support for hidden visibility")

   # get the gcc version
   exec_program(${CMAKE_C_COMPILER} ARGS --version OUTPUT_VARIABLE _gcc_version_info)

   string (REGEX MATCH "[345]\\.[0-9]\\.[0-9]" _gcc_version "${_gcc_version_info}")
   # gcc on mac just reports: "gcc (GCC) 3.3 20030304 ..." without the patch level, handle this here:
   if (NOT _gcc_version)
      string (REGEX REPLACE ".*\\(GCC\\).* ([34]\\.[0-9]) .*" "\\1.0" _gcc_version "${_gcc_version_info}")
   endif (NOT _gcc_version)

   macro_ensure_version("4.1.0" "${_gcc_version}" GCC_IS_NEWER_THAN_4_1)
   macro_ensure_version("4.2.0" "${_gcc_version}" GCC_IS_NEWER_THAN_4_2)
   macro_ensure_version("4.3.0" "${_gcc_version}" GCC_IS_NEWER_THAN_4_3)

   # save a little by making local statics not threadsafe
   # ### do not enable it for older compilers, see
   # ### http://gcc.gnu.org/bugzilla/show_bug.cgi?id=31806
   if (GCC_IS_NEWER_THAN_4_3)
       set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics")
   endif (GCC_IS_NEWER_THAN_4_3)

   set(_GCC_COMPILED_WITH_BAD_ALLOCATOR FALSE)
   if (GCC_IS_NEWER_THAN_4_1)
      exec_program(${CMAKE_C_COMPILER} ARGS -v OUTPUT_VARIABLE _gcc_alloc_info)
      string(REGEX MATCH "(--enable-libstdcxx-allocator=mt)" _GCC_COMPILED_WITH_BAD_ALLOCATOR "${_gcc_alloc_info}")
   endif (GCC_IS_NEWER_THAN_4_1)

   if (__KDE_HAVE_GCC_VISIBILITY AND GCC_IS_NEWER_THAN_4_1 AND NOT _GCC_COMPILED_WITH_BAD_ALLOCATOR)
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden")
      set (KDE4_C_FLAGS "-fvisibility=hidden")
      # check that Qt defines Q_DECL_EXPORT as __attribute__ ((visibility("default")))
      # if it doesn't and KDE compiles with hidden default visibiltiy plugins will break
      set(_source "#include <QtCore/QtGlobal>\n int main()\n {\n #ifdef QT_VISIBILITY_AVAILABLE \n return 0;\n #else \n return 1; \n #endif \n }\n")
      set(_source_file ${CMAKE_BINARY_DIR}/CMakeTmp/check_qt_visibility.cpp)
      file(WRITE "${_source_file}" "${_source}")
      set(_include_dirs "-DINCLUDE_DIRECTORIES:STRING=${QT_INCLUDES}")

      try_run(_run_result _compile_result ${CMAKE_BINARY_DIR} ${_source_file} CMAKE_FLAGS "${_include_dirs}")

      if(NOT _compile_result)
         message(FATAL_ERROR "Could not compile simple test program:\n ${_source}")
      endif(NOT _compile_result)
      if(_run_result)
         message(FATAL_ERROR "Qt compiled without support for -fvisibility=hidden. This will break plugins and linking of some applications. Please fix your Qt installation.")
      endif(_run_result)

      if (GCC_IS_NEWER_THAN_4_2)
          set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility-inlines-hidden")
      endif (GCC_IS_NEWER_THAN_4_2)
   else (__KDE_HAVE_GCC_VISIBILITY AND GCC_IS_NEWER_THAN_4_1 AND NOT _GCC_COMPILED_WITH_BAD_ALLOCATOR)
      set (__KDE_HAVE_GCC_VISIBILITY 0)
   endif (__KDE_HAVE_GCC_VISIBILITY AND GCC_IS_NEWER_THAN_4_1 AND NOT _GCC_COMPILED_WITH_BAD_ALLOCATOR)

endif (CMAKE_COMPILER_IS_GNUCXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE ON)
set(CMAKE_COLOR_MAKEFILE ON)

set(PHONON_LIB_MAJOR_VERSION "4")
set(PHONON_LIB_MINOR_VERSION "3")
set(PHONON_LIB_PATCH_VERSION "50")
set(PHONON_LIB_VERSION "${PHONON_LIB_MAJOR_VERSION}.4.0")
set(PHONON_LIB_SOVERSION ${PHONON_LIB_MAJOR_VERSION})

add_definitions(${QT_DEFINITIONS})
remove_definitions(-DQT3_SUPPORT_WARNINGS -DQT3_SUPPORT)
if(MSVC)
  add_definitions( -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS )
endif(MSVC)

# for including config.h and for includes like <kparts/foo.h>
include_directories(${QT_INCLUDES} ${PHONON_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/includes ${CMAKE_CURRENT_SOURCE_DIR}/phonon ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR}/phonon)

macro(_SET_FANCY _var _value _comment)
   if (KDESupport_SOURCE_DIR)
      # when building inside kdesupport other subprojects write crap into our variables
      set(${_var} "${_value}")
   else (KDESupport_SOURCE_DIR)
      if (NOT DEFINED ${_var})
         set(${_var} "${_value}")
      else (NOT DEFINED ${_var})
         set(${_var} "${${_var}}" CACHE PATH "${_comment}")
      endif (NOT DEFINED ${_var})
   endif (KDESupport_SOURCE_DIR)
endmacro(_SET_FANCY)

set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)" )

_set_fancy(EXEC_INSTALL_PREFIX         "${CMAKE_INSTALL_PREFIX}"                   "Base directory for executables and libraries")
_set_fancy(SHARE_INSTALL_PREFIX        "${CMAKE_INSTALL_PREFIX}/share"             "Base directory for files which go to share/")
_set_fancy(BIN_INSTALL_DIR             "${EXEC_INSTALL_PREFIX}/bin"                "The install dir for executables (default ${EXEC_INSTALL_PREFIX}/bin)")
_set_fancy(LIB_INSTALL_DIR             "${EXEC_INSTALL_PREFIX}/lib${LIB_SUFFIX}"   "The subdirectory relative to the install prefix where libraries will be installed (default is ${EXEC_INSTALL_PREFIX}/lib${LIB_SUFFIX})")
_set_fancy(INCLUDE_INSTALL_DIR         "${CMAKE_INSTALL_PREFIX}/include"           "The subdirectory to the header prefix")
_set_fancy(PLUGIN_INSTALL_DIR          "${LIB_INSTALL_DIR}/kde4"                   "The subdirectory relative to the install prefix where plugins will be installed (default is ${LIB_INSTALL_DIR}/kde4)")
_set_fancy(ICON_INSTALL_DIR            "${SHARE_INSTALL_PREFIX}/icons"             "The icon install dir (default ${SHARE_INSTALL_PREFIX}/share/icons/)")
_set_fancy(SERVICES_INSTALL_DIR        "${SHARE_INSTALL_PREFIX}/kde4/services"     "The install dir for service (desktop, protocol, ...) files")
_set_fancy(DBUS_INTERFACES_INSTALL_DIR "${SHARE_INSTALL_PREFIX}/dbus-1/interfaces" "The dbus interfaces install dir (default: ${SHARE_INSTALL_PREFIX}/dbus-1/interfaces)")
_set_fancy(DBUS_SERVICES_INSTALL_DIR   "${SHARE_INSTALL_PREFIX}/dbus-1/services"   "The dbus services install dir (default: ${SHARE_INSTALL_PREFIX}/dbus-1/services)")

set(INSTALL_TARGETS_DEFAULT_ARGS  RUNTIME DESTINATION "${BIN_INSTALL_DIR}"
                                  LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
                                  ARCHIVE DESTINATION "${LIB_INSTALL_DIR}" COMPONENT Devel )

# on the Mac support an extra install directory for application bundles
if(APPLE)
   set(INSTALL_TARGETS_DEFAULT_ARGS  ${INSTALL_TARGETS_DEFAULT_ARGS}
                                     BUNDLE DESTINATION "${BUNDLE_INSTALL_DIR}" )
	set(CMAKE_SHARED_MODULE_CREATE_C_FLAGS   "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS}   -flat_namespace -undefined dynamic_lookup")
	set(CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS "${CMAKE_SHARED_MODULE_CREATE_CXX_FLAGS} -flat_namespace -undefined dynamic_lookup")
endif(APPLE)

if (CMAKE_SYSTEM_NAME MATCHES Linux)
   if (CMAKE_COMPILER_IS_GNUCXX)
      set ( CMAKE_SHARED_LINKER_FLAGS "-Wl,--fatal-warnings -Wl,--no-undefined -lc ${CMAKE_SHARED_LINKER_FLAGS}")
      set ( CMAKE_MODULE_LINKER_FLAGS "-Wl,--fatal-warnings -Wl,--no-undefined -lc ${CMAKE_MODULE_LINKER_FLAGS}")

      set ( CMAKE_SHARED_LINKER_FLAGS "-Wl,--enable-new-dtags ${CMAKE_SHARED_LINKER_FLAGS}")
      set ( CMAKE_MODULE_LINKER_FLAGS "-Wl,--enable-new-dtags ${CMAKE_MODULE_LINKER_FLAGS}")
      set ( CMAKE_EXE_LINKER_FLAGS "-Wl,--enable-new-dtags ${CMAKE_EXE_LINKER_FLAGS}")

      # we profile...
      if(CMAKE_BUILD_TYPE_TOLOWER MATCHES profile)
        set (CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
        set (CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
      endif(CMAKE_BUILD_TYPE_TOLOWER MATCHES profile)
   endif (CMAKE_COMPILER_IS_GNUCXX)
   if (CMAKE_C_COMPILER MATCHES "icc")
      set ( CMAKE_SHARED_LINKER_FLAGS "-Wl,--fatal-warnings -Wl,--no-undefined -lc ${CMAKE_SHARED_LINKER_FLAGS}")
      set ( CMAKE_MODULE_LINKER_FLAGS "-Wl,--fatal-warnings -Wl,--no-undefined -lc ${CMAKE_MODULE_LINKER_FLAGS}")
   endif (CMAKE_C_COMPILER MATCHES "icc")
endif (CMAKE_SYSTEM_NAME MATCHES Linux)

list(APPEND PHONON_LIBS phonon ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY})
if(QT_QTDBUS_FOUND)
    list(APPEND PHONON_LIBS phonon ${QT_QTDBUS_LIBRARY})
endif(QT_QTDBUS_FOUND)

set(EXECUTABLE_OUTPUT_PATH ${Phonon_BINARY_DIR}/bin)
if (WIN32)
   set(LIBRARY_OUTPUT_PATH ${EXECUTABLE_OUTPUT_PATH})
else (WIN32)
   set(LIBRARY_OUTPUT_PATH ${Phonon_BINARY_DIR}/lib)
endif (WIN32)

if (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER MATCHES "icc")
   set ( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor -Wno-long-long -ansi -Wundef -Wcast-align -Wchar-subscripts -Wall -W -Wpointer-arith -Wformat-security -fno-check-new -fno-common")
endif (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_C_COMPILER MATCHES "icc")

# Set up RPATH handling, so the libs are found if they are installed to a non-standard location.
# By default cmake builds the targets with full RPATH to everything in the build directory,
# but then removes the RPATH when installing.
# These two options below make it set the RPATH of the installed targets to all
# RPATH directories outside the current CMAKE_BINARY_DIR and also the library
# install directory. Alex
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH  TRUE)
set(CMAKE_INSTALL_RPATH "${LIB_INSTALL_DIR}" )

if(APPLE)
   set(CMAKE_INSTALL_NAME_DIR ${LIB_INSTALL_DIR})
endif(APPLE)

if (Q_WS_X11)
   add_subdirectory(vlc)
endif (Q_WS_X11)

macro_display_feature_log()
