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
	$(CC) $(CFLAGS) $(PCFLAGS) -I/usr/include/qt -I${QTDIR}/include -c -o $@ $(<:%.moc=%.cpp)
$(PLUGIN_QT:%.o=%.moc): %.moc: %.cpp
	$(MOC) -i $< -o $@

$(BUILTIN_QT): BUILTIN_%.o: .dep/%.dpp
$(BUILTIN_QT): %.o: %.moc
	$(CC) $(CFLAGS) -DBUILTIN -I/usr/include/qt -I${QTDIR}/include -c -o $@ $(<:BUILTIN_%.moc=%.cpp)
$(BUILTIN_QT:%.o=%.moc): BUILTIN_%.moc: %.cpp
	$(MOC) -i $< -o $@

#
# Real targets
#

../../lib/qt.so: $(PLUGIN_QT)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_QT)

../../lib/qt.a: $(BUILTIN_QT)
	ar r $@ $^
	$(RANLIB) $@

