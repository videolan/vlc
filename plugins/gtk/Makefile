###############################################################################
# vlc (VideoLAN Client) gtk/gnome module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_GTK = gtk.o intf_gtk.o gtk_interface.o gtk_support.o
PLUGIN_GNOME = gnome.o intf_gnome.o gnome_interface.o gnome_support.o gnome_callbacks.o

PLUGIN_GTK_SPECIFIC = gtk_display.o gtk_open.o gtk_control.o gtk_menu.o gtk_playlist.o gtk_modules.o gtk_preferences.o gtk_callbacks.o
PLUGIN_GNOME_SPECIFIC = $(PLUGIN_GTK_SPECIFIC:%.o=%_gnome.o)

BUILTIN_GTK = $(PLUGIN_GTK:%.o=BUILTIN_GTK_%.o) \
        $(PLUGIN_GTK_SPECIFIC:%.o=BUILTIN_GTK_%.o)
BUILTIN_GNOME = $(PLUGIN_GNOME:%.o=BUILTIN_GNOME_%.o) \
        $(PLUGIN_GTK_SPECIFIC:%.o=BUILTIN_GNOME_%.o)

ALL_OBJ = $(PLUGIN_GTK) $(PLUGIN_GNOME) $(PLUGIN_GTK_SPECIFIC) \
          $(PLUGIN_GNOME_SPECIFIC) $(BUILTIN_GTK) $(BUILTIN_GNOME)

#
# Virtual targets
#

include ../../Makefile.modules

# Gtk+
$(PLUGIN_GTK) $(PLUGIN_GTK_SPECIFIC): %.o: .dep/%.d
$(PLUGIN_GTK) $(PLUGIN_GTK_SPECIFIC): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) $(CFLAGS_GTK) -DPLUGIN -DMODULE_NAME=gtk -c -o $@ $<

$(BUILTIN_GTK): BUILTIN_GTK_%.o: .dep/%.d
$(BUILTIN_GTK): BUILTIN_GTK_%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_GTK) -DBUILTIN -DMODULE_NAME=gtk -c -o $@ $<

# Gnome
$(PLUGIN_GNOME): %.o: .dep/%.d
$(PLUGIN_GNOME): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -DPLUGIN -DMODULE_NAME=gnome `gnome-config --cflags gtk gnomeui` -c -o $@ $<

$(PLUGIN_GNOME_SPECIFIC): %_gnome.o: .dep/%.d
$(PLUGIN_GNOME_SPECIFIC): %_gnome.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -DPLUGIN -DMODULE_NAME=gnome `gnome-config --cflags gtk gnomeui` -c -o $@ $<

$(BUILTIN_GNOME): BUILTIN_GNOME_%.o: .dep/%.d
$(BUILTIN_GNOME): BUILTIN_GNOME_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=gnome `gnome-config --cflags gtk gnomeui` -c -o $@ $<


#
# Real targets
#

../gtk.so: $(PLUGIN_GTK) $(PLUGIN_GTK_SPECIFIC)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_GTK)

../gtk.a: $(BUILTIN_GTK)
	ar r $@ $^
	$(RANLIB) $@


../gnome.so: $(PLUGIN_GNOME) $(PLUGIN_GNOME_SPECIFIC)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_GNOME)

../gnome.a: $(BUILTIN_GNOME)
	ar r $@ $^
	$(RANLIB) $@

