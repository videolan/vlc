###############################################################################
# vlc (VideoLAN Client) arts module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_C = arts.o aout_arts.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Special plugin modifications
#

CFLAGS += $(CFLAGS_ARTS)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../arts.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_ARTS)

../arts.a: $(BUILTIN_C)
	ar r $@ $^
	$(RANLIB) $@

