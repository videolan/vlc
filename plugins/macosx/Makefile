###############################################################################
# vlc (VideoLAN Client) macosx module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_MACOSX = macosx.o intf_macosx.o vout_macosx.o
PLUGIN_MACOSXQT = macosx_qt.o intf_macosx_qt.o vout_macosx_qt.o
PLUGIN_MACOSXCOMMON = aout_macosx.o

BUILTIN_MACOSX = $(PLUGIN_MACOSX:%.o=BUILTIN_MACOSX_%.o) \
		$(PLUGIN_MACOSXCOMMON:%.o=BUILTIN_MACOSX_%.o)
BUILTIN_MACOSXQT = $(PLUGIN_MACOSXQT:%.o=BUILTIN_MACOSXQT_%.o)  \
		$(PLUGIN_MACOSXCOMMON:%.o=BUILTIN_MACOSXQT_%.o)

ALL_OBJ = $(PLUGIN_MACOSX) $(BUILTIN_MACOSX) $(PLUGIN_MACOSXQT) $(BUILTIN_MACOSXQT) $(PLUGIN_MACOSXCOMMON)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_MACOSX) $(PLUGIN_MACOSXQT) $(PLUGIN_MACOSXCOMMON): %.o: .dep/%.d
$(PLUGIN_MACOSX): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -fpascal-strings -c -o $@ $<

$(BUILTIN_MACOSX): BUILTIN_MACOSX_%.o: .dep/%.d
$(BUILTIN_MACOSX): BUILTIN_MACOSX_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=macosx -fpascal-strings -c -o $@ $<

$(BUILTIN_MACOSXQT): BUILTIN_MACOSXQT_%.o: .dep/%.d
$(BUILTIN_MACOSXQT): BUILTIN_MACOSXQT_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=macosx_qt -fpascal-strings -c -o $@ $<

#
# Real targets
#

../macosx.so: $(PLUGIN_MACOSX) $(PLUGIN_MACOSXCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_MACOSX)

../macosx.a: $(BUILTIN_MACOSX)
	ar r $@ $^
	$(RANLIB) $@

../macosx_qt.so: $(PLUGIN_MACOSXQT) $(PLUGIN_MACOSXCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_MACOSX)

../macosx_qt.a: $(BUILTIN_MACOSXQT)
	ar r $@ $^
	$(RANLIB) $@

