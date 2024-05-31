TEMPLATE = app

QT = core gui qml svg quick widgets quickcontrols2
QTPLUGIN = qsvgicon qsvg qjpeg qico

CONFIG -= entrypoint
CONFIG -= debug_and_release
CONFIG += no_include_pwd

win32 {
QTPLUGIN += qwindows qmodernwindowsstyle
}

linux {
QT += opengl # compositor_x11 requires Qt OpenGL
QTPLUGIN += qxcb-glx-integration qxcb-egl-integration qxcb qwayland-generic qwayland-egl qgtk3 qxdgdesktopportal xdg-shell
}

macx {
QTPLUGIN += qcocoa qmacstyle
}

# These are not needed now, but may be required in the future:
# CONFIG += import_plugins staticlib create_pc create_prl no_install_prl link_prl
