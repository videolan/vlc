###############################################################################
# vlc (VideoLAN Client) macosx module makefile
# (c)2001 VideoLAN
###############################################################################

-include ../../Makefile.modules

#
# Objects
#

PLUGIN_MACOSX = macosx.o intf_macosx.o aout_macosx.o vout_macosx.o
BUILTIN_MACOSX = $(PLUGIN_MACOSX:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_MACOSX) $(BUILTIN_MACOSX)

#
# Virtual targets
#

include ../../Makefile.common

$(PLUGIN_MACOSX): %.o: .dep/%.d
$(PLUGIN_MACOSX): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -fpascal-strings -c -o $@ $<

$(BUILTIN_MACOSX): BUILTIN_%.o: .dep/%.d
$(BUILTIN_MACOSX): BUILTIN_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -fpascal-strings -c -o $@ $<

#
# Real targets
#

../../lib/macosx.so: $(PLUGIN_MACOSX)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_MACOSX)

../../lib/macosx.a: $(BUILTIN_MACOSX)
	ar r $@ $^

