###############################################################################
# vlc (VideoLAN Client) gnome module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_GNOME = gnome.o intf_gnome.o gnome_callbacks.o gnome_interface.o gnome_support.o
BUILTIN_GNOME = $(PLUGIN_GNOME:%.o=%-BUILTIN.o)

ALL_OBJ = $(PLUGIN_GNOME) $(BUILTIN_GNOME)

objects := $(PLUGIN_GNOME) $(BUILTIN_GNOME)
cdependancies := $(objects:%.o=.dep/%.d)

export

#
# Virtual targets
#
all:

clean:
	rm -f $(ALL_OBJ)
	rm -f *.o *.moc *.bak *.builtin
	rm -rf .dep

FORCE:

$(cdependancies): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(ALL_OBJ): %.o: ../../Makefile.dep Makefile

$(PLUGIN_GNOME): %.o: .dep/%.d
$(PLUGIN_GNOME): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) `gnome-config --cflags gtk gnomeui` -c -o $@ $<

$(BUILTIN_GNOME): %-BUILTIN.o: .dep/%.d
$(BUILTIN_GNOME): %-BUILTIN.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN `gnome-config --cflags gtk gnomeui` -c -o $@ $<

#
# Real targets
#
../../lib/gnome.so: $(PLUGIN_GNOME)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) `gnome-config --libs gnomeui | sed 's,-rdynamic,,'`

../../lib/gnome.a: $(BUILTIN_GNOME)
	ar r $@ $^

