###############################################################################
# vlc (VideoLAN Client) kde module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#
 
MOCFILES =	kinterfacemain.moc.cpp \
		kvlcslider.moc.cpp \
		kdiskdialog.moc.cpp \
		knetdialog.moc.cpp \
		ktitlemenu.moc.cpp

PLUGIN_KDE =	kde.o \
		kdeinterface.o \
		kinterfacemain.o \
		kvlcslider.o \
		kdiskdialog.o \
		knetdialog.o \
		ktitlemenu.o

KDE_CFLAGS = -I/usr/include/kde -I/usr/include/qt
ifneq ($(KDEDIR),)
  KDE_CFLAGS += -I$(KDEDIR)/include -I$(QTDIR)/include
endif

PLUGIN_KDE_MOC = $(MOCFILES:%.cpp=%.o)

BUILTIN_KDE = $(PLUGIN_KDE:%.o=BUILTIN_%.o)
BUILTIN_KDE_MOC = $(MOCFILES:%.cpp=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_KDE) $(PLUGIN_KDE_MOC) $(BUILTIN_KDE) $(BUILTIN_KDE_MOC)

#
# Virtual targets
#

include ../../Makefile.modules

$(MOCFILES): %.moc.cpp: %.h
	$(MOC) $< -o $@

$(PLUGIN_KDE): %.o: .dep/%.dpp
$(PLUGIN_KDE): %.o: %.cpp
	$(CC) $(CFLAGS) $(PCFLAGS) $(KDE_CFLAGS) -DPLUGIN -DMODULE_NAME=kde -c -o $@ $<

$(PLUGIN_KDE_MOC): %.o: .dep/%.dpp
$(PLUGIN_KDE_MOC): %.o: %.cpp
	$(CC) $(CFLAGS) $(PCFLAGS) $(KDE_CFLAGS) -DPLUGIN -DMODULE_NAME=kde -c -o $@ $<

$(BUILTIN_KDE): BUILTIN_%.o: .dep/%.dpp
$(BUILTIN_KDE): BUILTIN_%.o: %.cpp
	$(CC) $(CFLAGS) -DBUILTIN $(KDE_CFLAGS) -DBUILTIN -DMODULE_NAME=kde -c -o $@ $<

$(BUILTIN_KDE_MOC): BUILTIN_%.o: .dep/%.dpp
$(BUILTIN_KDE_MOC): BUILTIN_%.o: %.cpp
	$(CC) $(CFLAGS) -DBUILTIN $(KDE_CFLAGS) -DBUILTIN -DMODULE_NAME=kde -c -o $@ $<

#
# Real targets
#

../kde.so: $(PLUGIN_KDE) $(PLUGIN_KDE_MOC)
	$(CC) -o $@ $^ $(PLCFLAGS) $(LIB_KDE)

../kde.a: $(BUILTIN_KDE) $(BUILTIN_KDE_MOC)
	ar r $@ $^
	$(RANLIB) $@

