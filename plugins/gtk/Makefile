###############################################################################
# vlc (VideoLAN Client) gtk module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_GTK = gtk.o intf_gtk.o gtk_callbacks.o gtk_interface.o gtk_support.o gtk_playlist.o
BUILTIN_GTK = $(PLUGIN_GTK:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_GTK) $(BUILTIN_GTK)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_GTK): %.o: .dep/%.d
$(PLUGIN_GTK): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) `gtk-config --cflags gtk` -c -o $@ $<

$(BUILTIN_GTK): BUILTIN_%.o: .dep/%.d
$(BUILTIN_GTK): BUILTIN_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN `gtk-config --cflags gtk` -c -o $@ $<

#
# Real targets
#

../../lib/gtk.so: $(PLUGIN_GTK)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_GTK)

../../lib/gtk.a: $(BUILTIN_GTK)
	ar r $@ $^

