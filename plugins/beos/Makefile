###############################################################################
# vlc (VideoLAN Client) BeOS module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_BEOS = beos.o aout_beos.o vout_beos.o intf_beos.o DrawingTidbits.o TransportButton.o
BUILTIN_BEOS = $(PLUGIN_BEOS:%.o=%-BUILTIN.o)

NONSTD_CPP_PLUGIN_OBJ = $(PLUGIN_BEOS)
NONSTD_CPP_BUILTIN_OBJ = $(BUILTIN_BEOS)
ALL_OBJ = $(NONSTD_CPP_PLUGIN_OBJ) $(NONSTD_CPP_BUILTIN_OBJ)

cppobjects := $(NONSTD_CPP_PLUGIN_OBJ) $(NONSTD_CPP_BUILTIN_OBJ)
cppdependancies := $(cppobjects:%.o=.dep/%.dpp)

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

$(cppdependancies): %.dpp: FORCE
	@$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(ALL_OBJ): %.o: ../../Makefile.dep Makefile

$(PLUGIN_BEOS): %.o: .dep/%.dpp
$(PLUGIN_BEOS): %.o: %.cpp
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

$(BUILTIN_BEOS): %-BUILTIN.o: .dep/%.dpp
$(BUILTIN_BEOS): %-BUILTIN.o: %.cpp
	$(CC) $(CFLAGS) -DBUILTIN -c -o $@ $<

#
# Real targets
#
../../lib/beos.so: $(PLUGIN_BEOS)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -lbe -lgame -lroot -ltracker

../../lib/beos.a: $(BUILTIN_BEOS)
	ar r $@ $^

