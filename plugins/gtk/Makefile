###############################################################################
# vlc (VideoLAN Client) gtk module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_C = gtk.o intf_gtk.o gtk_callbacks.o gtk_interface.o gtk_support.o gtk_open.o gtk_control.o gtk_menu.o gtk_playlist.o gtk_modules.o gtk_preferences.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Special plugin modifications
#

CFLAGS += $(CFLAGS_GTK)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../lib/gtk.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_GTK)

../../lib/gtk.a: $(BUILTIN_C)
	ar r $@ $^
	$(RANLIB) $@

