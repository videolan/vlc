TEMPLATE = app

QT = core gui qml svg quick widgets quickcontrols2
QTPLUGIN = qsvgicon qsvg qjpeg qico

CONFIG -= entrypoint

win32 {
QTPLUGIN += qwindows qmodernwindowsstyle
}

linux {
QTPLUGIN += qxcb-glx-integration qxcb-egl-integration qxcb qwayland-generic qwayland-egl qgtk3 qxdgdesktopportal xdg-shell
}

macx {
QTPLUGIN += qcocoa qmacstyle
}

# qmake will run qmlimportscanner,
# which will make it generate correct qml plugin
# dependencies
RESOURCES = imports.qrc

# These are not needed now, but may be required in the future:
# CONFIG += import_plugins staticlib create_pc create_prl no_install_prl link_prl
