###############################################################################
# vlc (VideoLAN Client) gtk/gnome module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_GTK = gtk.o intf_gtk.o gtk_interface.o gtk_support.o
PLUGIN_GNOME = gnome.o intf_gnome.o gnome_interface.o gnome_support.o gnome_callbacks.o
PLUGIN_GTKCOMMON = gtk_display.o gtk_callbacks.o gtk_open.o gtk_control.o gtk_menu.o gtk_playlist.o gtk_modules.o gtk_preferences.o

BUILTIN_GTK = $(PLUGIN_GTK:%.o=BUILTIN_GTK_%.o) \
        $(PLUGIN_GTKCOMMON:%.o=BUILTIN_GTK_%.o)
BUILTIN_GNOME = $(PLUGIN_GNOME:%.o=BUILTIN_GNOME_%.o) \
        $(PLUGIN_GTKCOMMON:%.o=BUILTIN_GNOME_%.o)

ALL_OBJ = $(PLUGIN_GTK) $(PLUGIN_GNOME) $(PLUGIN_GTKCOMMON) \
          $(BUILTIN_GTK) $(BUILTIN_GNOME)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_GTKCOMMON): %.o: .dep/%.d
$(PLUGIN_GTKCOMMON): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) $(CFLAGS_GTK) -c -o $@ $<

$(PLUGIN_GTK): %.o: .dep/%.d
$(PLUGIN_GTK): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) $(CFLAGS_GTK) -DMODULE_NAME=gtk -c -o $@ $<

$(PLUGIN_GNOME): %.o: .dep/%.d
$(PLUGIN_GNOME): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -DMODULE_NAME=gnome `gnome-config --cflags gtk gnomeui` -c -o $@ $<

$(BUILTIN_GTK): BUILTIN_GTK_%.o: .dep/%.d
$(BUILTIN_GTK): BUILTIN_GTK_%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_GTK) -DBUILTIN -DMODULE_NAME=gtk -c -o $@ $<

$(BUILTIN_GNOME): BUILTIN_GNOME_%.o: .dep/%.d
$(BUILTIN_GNOME): BUILTIN_GNOME_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=gnome `gnome-config --cflags gtk gnomeui` -c -o $@ $<


#
# Real targets
#

../../lib/gtk.so: $(PLUGIN_GTK) $(PLUGIN_GTKCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_GTK)

../../lib/gtk.a: $(BUILTIN_GTK)
	ar r $@ $^
	$(RANLIB) $@


../../lib/gnome.so: $(PLUGIN_GNOME) $(PLUGIN_GTKCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_GNOME)

../../lib/gnome.a: $(BUILTIN_GNOME)
	ar r $@ $^
	$(RANLIB) $@
