###############################################################################
# vlc (VideoLAN Client) qt module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_QT = qt.o intf_qt.o
BUILTIN_QT = $(PLUGIN_QT:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_QT) $(BUILTIN_QT)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_QT): %.o: .dep/%.dpp
$(PLUGIN_QT): %.o: %.moc
	$(CC) $(CFLAGS) -DPLUGIN $(PCFLAGS) -I/usr/include/qt -I${QTDIR}/include -DPLUGIN -DMODULE_NAME=qt -c -o $@ $(<:%.moc=%.cpp)
$(PLUGIN_QT:%.o=%.moc): %.moc: %.cpp
	grep -q Q_OBJECT $< && $(MOC) -i $< -o $@ || true

$(BUILTIN_QT): BUILTIN_%.o: .dep/%.dpp
$(BUILTIN_QT): %.o: %.moc
	$(CC) $(CFLAGS) -DBUILTIN -I/usr/include/qt -I${QTDIR}/include -DBUILTIN -DMODULE_NAME=qt -c -o $@ $(<:BUILTIN_%.moc=%.cpp)
$(BUILTIN_QT:%.o=%.moc): BUILTIN_%.moc: %.cpp
	grep -q Q_OBJECT $< && $(MOC) -i $< -o $@ || true

#
# Real targets
#

../qt.so: $(PLUGIN_QT)
	$(CC) -o $@ $^ $(PLCFLAGS) $(LIB_QT)

../qt.a: $(BUILTIN_QT)
	ar r $@ $^
	$(RANLIB) $@

