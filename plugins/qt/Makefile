###############################################################################
# vlc (VideoLAN Client) qt module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_QT = qt.o intf_qt.o
BUILTIN_QT = $(PLUGIN_QT:%.o=%-BUILTIN.o)

ALL_OBJ = $(PLUGIN_QT) $(BUILTIN_QT)

cppobjects := $(ALL_OBJ)
cppdependancies := $(cppobjects:%.o=.dep/%.dpp)

export

#
# Virtual targets
#
all:

clean:
	rm -f $(ALL_OBJ)
	rm -f *.o *.moc *.bak *.so *.a *.builtin
	rm -rf .dep

FORCE:

$(cppdependancies): %.dpp: FORCE
	@$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(ALL_OBJ): %.o: ../../Makefile.dep

$(PLUGIN_QT): %.o: .dep/%.dpp
$(PLUGIN_QT): %.o: %.moc
	$(CC) $(CFLAGS) $(PCFLAGS) -I/usr/include/qt -I${QTDIR}/include -c -o $@ $(<:%.moc=%.cpp)
$(PLUGIN_QT:%.o=%.moc): %.moc: %.cpp
	moc -i $< -o $@

$(BUILTIN_QT): %.o: .dep/%.dpp
$(BUILTIN_QT): %.o: %.moc
	$(CC) $(CFLAGS) -DBUILTIN -I/usr/include/qt -I${QTDIR}/include -c -o $@ $(<:%.moc=%.cpp)
$(BUILTIN_QT:%.o=%.moc): %.moc: %.cpp
	moc -i $< -o $@

#
# Real targets
#
../../lib/qt.so: $(PLUGIN_QT)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -lqt -L${QTDIR}/lib

../../lib/qt.a: $(BUILTIN_QT)
	ar r $@ $^

