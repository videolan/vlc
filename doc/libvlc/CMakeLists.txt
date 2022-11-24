CMAKE_MINIMUM_REQUIRED(VERSION 3.0)

# configure with your own path
#  -DLIBVLC_SDK_INC:STRING=S:/sources/build/win64/win64/vlc-4.0.0-dev/sdk/include
#  -DLIBVLC_SDK_LIB:STRING=S:/sources/build/win64/win64/vlc-4.0.0-dev/sdk/lib
#
# or set them in your VSCode settings
# {
#     "cmake.configureSettings": {
#         "LIBVLC_SDK_INC": "S:/sources/vlc/include",
#         "LIBVLC_SDK_LIB": "S:/sources/build/win64/win64/lib/.libs"
#     }
# }

set("LIBVLC_SDK_INC" "" CACHE PATH "libvlc include folder, containing the vlc/ includes")
set("LIBVLC_SDK_LIB" "" CACHE PATH "libvlc library folder, containing the libvlc libraries")

project("libvlc samples")

# define the libvlc external build
add_library(libvlc SHARED IMPORTED GLOBAL)
target_include_directories(libvlc INTERFACE ${LIBVLC_SDK_INC})
if (MSVC)
    set_target_properties(libvlc PROPERTIES IMPORTED_IMPLIB "${LIBVLC_SDK_LIB}/libvlc.lib")
else ()
    set_target_properties(libvlc PROPERTIES IMPORTED_IMPLIB "${LIBVLC_SDK_LIB}/libvlc.dll.a")
endif ()

if(WIN32)

    add_executable(d3d9_player WIN32 d3d9_player.c)
    target_link_libraries(d3d9_player libvlc d3d9)

    add_executable(d3d11_player WIN32 d3d11_player.cpp)
    target_compile_definitions(d3d11_player PRIVATE _WIN32_WINNT=0x0601)
    target_link_libraries(d3d11_player libvlc d3d11 d3dcompiler uuid)

    add_executable(win_player WIN32 win_player.c)
    target_link_libraries(win_player libvlc)

endif()
