###############################################################################
# vlc (VideoLAN Client) kde module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#
 
PLUGIN_KDE =	kde.o \
		kdeinterface.o \
		kinterfacemain.o \
		kvlcslider.o \
		kdiskdialog.o \
		knetdialog.o \
		ktitlemenu.o

PLUGIN_KDE_MOC =	kinterfacemain.moc.o \
			kvlcslider.moc.o \
			kdiskdialog.moc.o \
			knetdialog.moc.o \
			ktitlemenu.moc.o

#BUILTIN_KDE = $(PLUGIN_KDE:%.o=BUILTIN_%.o)
#BUILTIN_KDE_MOC = $(PLUGIN_KDE_MOC:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_KDE) $(PLUGIN_KDE_MOC) $(BUILTIN_KDE) $(BUILTIN_KDE_MOC)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_KDE): %.o: .dep/%.dpp
$(PLUGIN_KDE): %.o: %.cpp
	$(CC) $(CFLAGS) $(PCFLAGS) -I/usr/include/kde -I/usr/include/qt -c -o $@ $<

$(PLUGIN_KDE_MOC): %.o: .dep/%.dpp
$(PLUGIN_KDE_MOC): %.o: %.cpp
	$(CC) $(CFLAGS) $(PCFLAGS) -I/usr/include/kde -I/usr/include/qt -c -o $@ $<
$(PLUGIN_KDE_MOC:%.moc.o=%.moc.cpp): %.moc.cpp: %.h
	moc $< -o $@

#$(BUILTIN_KDE): BUILTIN_%.o: .dep/%.dpp
#$(BUILTIN_KDE): BUILTIN_%.o: %.cpp
#	$(CC) $(CFLAGS) -DBUILTIN -I/usr/include/kde -I/usr/include/qt -c -o $@ $<

#$(BUILTIN_KDE_MOC): BUILTIN_%.o: dep/%.dpp
#$(BUILTIN_KDE_MOC): BUILTIN_%.o: %.cpp
#	$(CC) $(CFLAGS) -DBUILTIN -I/usr/include/kde -I/usr/include/qt -c -o $@ $<
#$(BUILTIN_KDE_MOC: %.moc.o=%.cpp): %.moc.cpp: %.h
#	$(MOC) $< -o $@

#
# Real targets
#

../../lib/kde.so: $(PLUGIN_KDE) $(PLUGIN_KDE_MOC)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_KDE) -lkfile

#../../lib/kde.a: $(BUILTIN_KDE)
#	ar r $@ $^
#	$(RANLIB) $@

