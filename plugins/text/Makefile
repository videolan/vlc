###############################################################################
# vlc (VideoLAN Client) text module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_NCURSES = ncurses.o intf_ncurses.o
PLUGIN_RC = rc.o intf_rc.o
BUILTIN_NCURSES = $(PLUGIN_NCURSES:%.o=BUILTIN_%.o)
BUILTIN_RC = $(PLUGIN_RC:%.o=BUILTIN_%.o)

PLUGIN_C = $(PLUGIN_NCURSES) $(PLUGIN_RC)
BUILTIN_C = $(BUILTIN_NCURSES) $(BUILTIN_RC)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../lib/ncurses.so: $(PLUGIN_NCURSES)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_NCURSES)

../../lib/ncurses.a: $(BUILTIN_NCURSES)
	ar r $@ $^

../../lib/rc.so: $(PLUGIN_RC)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS)

../../lib/rc.a: $(BUILTIN_RC)
	ar r $@ $^

