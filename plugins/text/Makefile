###############################################################################
# vlc (VideoLAN Client) text module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_NCURSES = ncurses.o vout_ncurses.o
BUILTIN_NCURSES = $(PLUGIN_NCURSES:%.o=%-BUILTIN.o)

ALL_OBJ = $(PLUGIN_NCURSES) $(BUILTIN_NCURSES)

objects := $(ALL_OBJ)
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

$(PLUGIN_NCURSES): %.o: .dep/%.d
$(PLUGIN_NCURSES): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

$(BUILTIN_NCURSES): %-BUILTIN.o: .dep/%.d
$(BUILTIN_NCURSES): %-BUILTIN.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -c -o $@ $<

#
# Real targets
#
../../lib/ncurses.so: $(PLUGIN_NCURSES)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -lncurses

../../lib/ncurses.a: $(BUILTIN_NCURSES)
	ar r $@ $^

