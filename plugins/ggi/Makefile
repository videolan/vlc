###############################################################################
# vlc (VideoLAN Client) ggi module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_C = ggi.o vout_ggi.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../plugins/ggi.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_GGI)

../../plugins/ggi.a: $(BUILTIN_C)
	ar r $@ $^
	$(RANLIB) $@

