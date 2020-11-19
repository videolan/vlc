TEMPLATE = app
TARGET = qtvlc

CONFIG += link_pkgconfig force_debug_info
PKGCONFIG = libvlc
QT += widgets

HEADERS += player.h
SOURCES += main.cpp player.cpp
