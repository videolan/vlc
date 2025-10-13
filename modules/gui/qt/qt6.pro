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

#Get default include paths for moc
#moc_incude_dirs is not a real command, it only serves as a marker for grepping
moc_include_dirs_target.commands = moc_include_dirs $$QMAKE_DEFAULT_INCDIRS
QMAKE_EXTRA_TARGETS += moc_include_dirs_target
