project(phonon-vlc)
include_directories(${QT_INCLUDES} 
                    ${VLC_INCLUDE_DIRS} 
                    ${CMAKE_CURRENT_SOURCE_DIR})

add_definitions(-D_PHONON_BACKEND_VERSION_4_2)

set(phonon_vlc_SRCS
    audiooutput.cpp
    backend.cpp
    devicemanager.cpp
    effect.cpp
    effectmanager.cpp
    mediacontroller.cpp
    mediaobject.cpp
    seekstack.cpp
    sinknode.cpp
    videowidget.cpp
    vlcloader.cpp
    vlcmediacontroller.cpp
    vlcmediaobject.cpp
    vlcvideowidget.cpp
    widgetnopaintevent.cpp)

automoc4(phonon_vlc phonon_vlc_SRCS)
add_library(phonon_vlc MODULE ${phonon_vlc_SRCS})

set_target_properties(phonon_vlc PROPERTIES PREFIX "")
target_link_libraries(phonon_vlc ${PHONON_LIBS} ${VLC_LIBRARIES})

if(WIN32)
    install(TARGETS phonon_vlc DESTINATION bin/phonon_backend)
else(WIN32)
    install(TARGETS phonon_vlc DESTINATION ${PLUGIN_INSTALL_DIR}/plugins/phonon_backend)
endif(WIN32)
install(FILES vlc.desktop DESTINATION ${SERVICES_INSTALL_DIR}/phononbackends)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif(BUILD_TESTS)
