###############################################################################
# vlc (VideoLAN Client) gnome module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_GNOME = gnome.o intf_gnome.o gnome_callbacks.o gnome_interface.o gnome_support.o
BUILTIN_GNOME = $(PLUGIN_GNOME:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_GNOME) $(BUILTIN_GNOME)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_GNOME): %.o: .dep/%.d
$(PLUGIN_GNOME): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) `gnome-config --cflags gtk gnomeui` -c -o $@ $<

$(BUILTIN_GNOME): BUILTIN_%.o: .dep/%.d
$(BUILTIN_GNOME): BUILTIN_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN `gnome-config --cflags gtk gnomeui` -c -o $@ $<

#
# Real targets
#

../../lib/gnome.so: $(PLUGIN_GNOME)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_GNOME)

../../lib/gnome.a: $(BUILTIN_GNOME)
	ar r $@ $^
	$(RANLIB) $@

