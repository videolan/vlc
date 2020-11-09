TEMPLATE = app
TARGET = qtglvlc

CONFIG += link_pkgconfig
PKGCONFIG = libvlc
QT += widgets

SOURCES += main.cpp qtvlcwidget.cpp
HEADERS += qtvlcwidget.h
