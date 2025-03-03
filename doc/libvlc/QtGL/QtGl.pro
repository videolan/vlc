TEMPLATE = app
TARGET = qtglvlc

CONFIG += c++14 link_pkgconfig force_debug_info
PKGCONFIG = libvlc
QT += widgets
greaterThan(QT_MAJOR_VERSION, 5): QT += openglwidgets

SOURCES += main.cpp qtvlcwidget.cpp
HEADERS += qtvlcwidget.h
