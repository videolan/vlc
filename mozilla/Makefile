###############################################################################
# vlc (VideoLAN Client) Mozilla plugin Makefile
# (c)2002 VideoLAN
###############################################################################

#
# Source objects
#
CPP_SRC = vlcplugin.cpp vlcpeer.cpp vlcshell.cpp
C_SRC = npunix.c
CPP_OBJ = $(CPP_SRC:%.cpp=%.o)
C_OBJ = $(C_SRC:%.c=%.o)

PLUGIN_OBJ = libvlcplugin.so
COMPONENT = vlcintf.xpt

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
	mkdir -p $(DESTDIR)$(libdir)/mozilla/components
	$(INSTALL) -m 644 $(COMPONENT) $(DESTDIR)$(libdir)/mozilla/components

uninstall:
	rm -f $(DESTDIR)$(libdir)/mozilla/plugins/$(PLUGIN_OBJ)
	-rmdir $(DESTDIR)$(libdir)/mozilla/plugins
	rm -f $(DESTDIR)$(libdir)/mozilla/components/$(COMPONENT)
	-rmdir $(DESTDIR)$(libdir)/mozilla/components
	-rmdir $(DESTDIR)$(libdir)/mozilla

FORCE:

$(PLUGIN_OBJ): Makefile ../lib/libvlc.a $(BUILTIN_OBJ:%=../%) $(C_OBJ) $(CPP_OBJ) $(COMPONENT)
	$(CC) -shared $(LDFLAGS) -L../lib $(mozilla_LDFLAGS) $(C_OBJ) $(CPP_OBJ) -lvlc $(BUILTIN_OBJ:%=../%) $(builtins_LDFLAGS) -o $@

$(CPP_OBJ): %.o: %.cpp vlcplugin.h vlcpeer.h vlcintf.h classinfo.h
	$(CC) $(CFLAGS) -I.. -I../include $(mozilla_CFLAGS) -c $< -o $@

$(C_OBJ): %.o: %.c vlcplugin.h vlcpeer.h vlcintf.h classinfo.h
	$(CC) $(CFLAGS) -I.. -I../include $(mozilla_CFLAGS) -c $< -o $@

vlcintf.xpt: vlcintf.idl
	/usr/lib/mozilla/xpidl -I/usr/share/idl/mozilla -m typelib \
		-o vlcintf vlcintf.idl

vlcintf.h: vlcintf.idl
	/usr/lib/mozilla/xpidl -I/usr/share/idl/mozilla -m header \
		-o vlcintf vlcintf.idl

../%:
	@cd .. && $(MAKE) $(@:../%=%)

