###############################################################################
# vlc (VideoLAN Client) sdl module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_C = sdl.o vout_sdl.o aout_sdl.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_SDL): %.o: .dep/%.d
$(PLUGIN_SDL): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) $(CFLAGS_SDL) -DPLUGIN -c -o $@ $<

$(BUILTIN_SDL): BUILTIN_SDL_%.o: .dep/%.d
$(BUILTIN_SDL): BUILTIN_SDL_%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_SDL) -DBUILTIN -c -o $@ $<

#
# Real targets
#

../../lib/sdl.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_SDL)

../../lib/sdl.a: $(BUILTIN_C)
	ar r $@ $^
	$(RANLIB) $@

