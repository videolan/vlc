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

emscripten {
QTPLUGIN += qwasm
}
