###############################################################################
# vlc (VideoLAN Client) sdl module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_SDL = sdl.o vout_sdl.o aout_sdl.o
BUILTIN_SDL = $(PLUGIN_SDL:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_SDL) $(BUILTIN_SDL)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_SDL): %.o: .dep/%.d
$(PLUGIN_SDL): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) $(CFLAGS_SDL) -DPLUGIN -c -o $@ $<

$(BUILTIN_SDL): BUILTIN_%.o: .dep/%.d
$(BUILTIN_SDL): BUILTIN_%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_SDL) -DBUILTIN -c -o $@ $<

#
# Real targets
#

../../plugins/sdl.so: $(PLUGIN_SDL)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_SDL)

../../plugins/sdl.a: $(BUILTIN_SDL)
	ar r $@ $^
	$(RANLIB) $@

