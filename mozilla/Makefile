###############################################################################
# vlc (VideoLAN Client) Mozilla plugin Makefile
# (c)2002 VideoLAN
###############################################################################

#
# Source objects
#
C_SRC = vlcplugin.c npunix.c
C_OBJ = $(C_SRC:%.c=%.o)

PLUGIN_OBJ = libvlcplugin.so

#
# Virtual targets
#
all: $(PLUGIN_OBJ)

distclean: clean

clean:
	rm -f *.o *.so
	rm -Rf .dep

install:
	mkdir -p $(DESTDIR)$(libdir)/mozilla/plugins
	$(INSTALL) -m 644 $(PLUGIN_OBJ) $(DESTDIR)$(libdir)/mozilla/plugins

uninstall:
	rm -f $(DESTDIR)$(libdir)/mozilla/plugins/$(PLUGIN_OBJ)
	-rmdir $(DESTDIR)$(libdir)/mozilla/plugins
	-rmdir $(DESTDIR)$(libdir)/mozilla

FORCE:

$(PLUGIN_OBJ): Makefile ../lib/libvlc.a $(BUILTIN_OBJ:%=../%) $(C_OBJ)
	$(CC) -shared $(LDFLAGS) -L../lib $(mozilla_LDFLAGS) $(C_OBJ) -lvlc $(BUILTIN_OBJ:%=../%) $(builtins_LDFLAGS) -o $@

$(C_OBJ): %.o: %.c vlcplugin.h
	$(CC) $(CFLAGS) -I../include $(mozilla_CFLAGS) -c $< -o $@

../%:
	@cd .. && $(MAKE) $(@:../%=%)

