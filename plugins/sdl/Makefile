###############################################################################
# vlc (VideoLAN Client) sdl module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_SDL = sdl.o vout_sdl.o aout_sdl.o
BUILTIN_SDL = $(PLUGIN_SDL:%.o=%-BUILTIN.o)

ALL_OBJ = $(PLUGIN_SDL) $(BUILTIN_SDL)

objects := $(ALL_OBJ)
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

$(PLUGIN_SDL): %.o: .dep/%.d
$(PLUGIN_SDL): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

$(BUILTIN_SDL): %-BUILTIN.o: .dep/%.d
$(BUILTIN_SDL): %-BUILTIN.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -c -o $@ $<

#
# Real targets
#
../../lib/sdl.so: $(PLUGIN_SDL)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_SDL)

../../lib/sdl.a: $(BUILTIN_SDL)
	ar r $@ $^

