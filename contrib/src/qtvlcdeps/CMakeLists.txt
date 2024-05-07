cmake_minimum_required(VERSION 3.16)

project(qtvlcdeps VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Qml Svg Quick Widgets QuickControls2)

qt_standard_project_setup(REQUIRES 6.2)

qt_add_executable(qtvlcdeps)

qt_add_qml_module(qtvlcdeps
    URI qtvlcdeps
    RESOURCE_PREFIX qtvlcdeps
    QML_FILES
        Imports.qml
)

qt_import_plugins(qtvlcdeps
    INCLUDE Qt::QSvgIconPlugin Qt::QSvgPlugin Qt::QJpegPlugin Qt::QICOPlugin
)

if (WIN32)
    qt_import_plugins(qtvlcdeps
        INCLUDE Qt::QWindowsIntegrationPlugin Qt::QModernWindowsStylePlugin
    )
endif (WIN32)

if (UNIX)
    qt_import_plugins(qtvlcdeps
        INCLUDE Qt::QXcbGlxIntegrationPlugin Qt::QXcbEglIntegrationPlugin Qt::QXcbIntegrationPlugin Qt::QWaylandIntegrationPlugin Qt::QWaylandEglPlatformIntegrationPlugin Qt::QGtk3ThemePlugin Qt::QXdgDesktopPortalThemePlugin Qt::QWaylandXdgShellPlatformIntegrationPlugin
    )
endif (UNIX)

if (APPLE)
    qt_import_plugins(qtvlcdeps
        INCLUDE Qt::QCocoaIntegrationPlugin Qt::QMacStylePlugin
    )
endif (APPLE)


set_target_properties(qtvlcdeps PROPERTIES
    WIN32_EXECUTABLE TRUE
    qt_no_entrypoint TRUE
)

target_link_libraries(qtvlcdeps
    PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Svg Qt6::Quick Qt6::Widgets Qt6::QuickControls2
)
