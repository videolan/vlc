TEMPLATE = app
TARGET = qtvlc
DEPENDPATH += .
INCLUDEPATH += .
LIBS += -lvlc -lX11

# Input
HEADERS += player.h
SOURCES += main.cpp player.cpp
