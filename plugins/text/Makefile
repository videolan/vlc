###############################################################################
# vlc (VideoLAN Client) text module Makefile
# (c)2001 VideoLAN
###############################################################################

-include ../../Makefile.modules

#
# Objects
#

PLUGIN_C = ncurses.o vout_ncurses.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.common

#
# Real targets
#

../../lib/ncurses.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_NCURSES)

../../lib/ncurses.a: $(BUILTIN_C)
	ar r $@ $^

