###############################################################################
# vlc (VideoLAN Client) macosx module makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_MACOSX = macosx.o intf_macosx.o aout_macosx.o vout_macosx.o
BUILTIN_MACOSX = $(PLUGIN_MACOSX:%.o=%-BUILTIN.o)

ALL_OBJ = $(PLUGIN_MACOSX) $(BUILTIN_MACOSX)

objects := $(PLUGIN_MACOSX) $(BUILTIN_MACOSX)
cdependancies := $(objects:%.o=.dep/%.d)

export

#
# Virtual targets
#
all:

clean:
	rm -f $(ALL_OBJ)
	rm -f *.o *.moc *.bak *.builtin
	rm -rf .dep

FORCE:

$(cdependancies): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(ALL_OBJ): %.o: ../../Makefile.dep Makefile

$(PLUGIN_MACOSX): %.o: .dep/%.d
$(PLUGIN_MACOSX): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -fpascal-strings -c -o $@ $<

$(BUILTIN_MACOSX): %-BUILTIN.o: .dep/%.d
$(BUILTIN_MACOSX): %-BUILTIN.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -fpascal-strings -c -o $@ $<

#
# Real targets
#
../../lib/macosx.so: $(PLUGIN_MACOSX)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -framework CoreAudio -framework Carbon -framework AGL

../../lib/macosx.a: $(BUILTIN_MACOSX)
	ar r $@ $^

