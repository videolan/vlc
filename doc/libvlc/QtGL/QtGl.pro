TEMPLATE = app
TARGET = qtglvlc

CONFIG += c++14 link_pkgconfig force_debug_info
PKGCONFIG = libvlc
QT += widgets

SOURCES += main.cpp qtvlcwidget.cpp
HEADERS += qtvlcwidget.h
