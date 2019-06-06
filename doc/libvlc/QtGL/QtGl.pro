TEMPLATE = app
TARGET = qtglvlc
DEPENDPATH += .
INCLUDEPATH += . ../../../include
LIBS += -lvlc
#-L../../../../build/git64/lib/.libs
QT += widgets

SOURCES += main.cpp qtvlcwidget.cpp
HEADERS += qtvlcwidget.h
