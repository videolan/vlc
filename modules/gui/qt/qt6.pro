TEMPLATE = app

QT = core gui qml quick widgets quickcontrols2
QTPLUGIN = qsvgicon qsvg qjpeg qico

CONFIG -= entrypoint
CONFIG -= debug_and_release
CONFIG += no_include_pwd

win32 {
QTPLUGIN += qwindows

versionAtLeast(QT_VERSION, 6.7.0) {
QTPLUGIN += qmodernwindowsstyle
} else {
QTPLUGIN += qwindowsvistastyle
}
}

linux {
QTPLUGIN += qxcb-glx-integration qxcb-egl-integration qxcb qwayland-generic qwayland-egl qgtk3 qxdgdesktopportal xdg-shell
}

macx {
QTPLUGIN += qcocoa qmacstyle
}

emscripten {
QTPLUGIN += qwasm
}

qtHaveModule(gui-private) {
QT += gui-private
DEFINES += QT_GUI_PRIVATE
}

qtHaveModule(quick-private) {
QT += quick-private
DEFINES += QT_DECLARATIVE_PRIVATE
}
