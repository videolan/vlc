###############################################################################
# vlc (VideoLAN Client) gtk module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_GTK = gtk.o intf_gtk.o gtk_callbacks.o gtk_interface.o gtk_support.o gtk_playlist.o
BUILTIN_GTK = $(PLUGIN_GTK:%.o=%-BUILTIN.o)

ALL_OBJ = $(PLUGIN_GTK) $(BUILTIN_GTK)

objects := $(PLUGIN_GTK) $(BUILTIN_GTK)
cdependancies := $(objects:%.o=.dep/%.d)

export

#
# Virtual targets
#
all:

clean:
	rm -f $(ALL_OBJ)
	rm -f *.o *.moc *.bak *.so *.a *.builtin
	rm -rf .dep

FORCE:

$(cdependancies): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(ALL_OBJ): %.o: ../../Makefile.dep

$(PLUGIN_GTK): %.o: .dep/%.d
$(PLUGIN_GTK): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) `gtk-config --cflags gtk` -c -o $@ $<

$(BUILTIN_GTK): %-BUILTIN.o: .dep/%.d
$(BUILTIN_GTK): %-BUILTIN.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN `gtk-config --cflags gtk` -c -o $@ $<

#
# Real targets
#
../../lib/gtk.so: $(PLUGIN_GTK)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) `gtk-config --libs gtk | sed 's,-rdynamic,,'`

../../lib/gtk.a: $(BUILTIN_GTK)
	ar r $@ $^

