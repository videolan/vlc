###############################################################################
# vlc (VideoLAN Client) mga module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_C = mga.o vout_mga.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../lib/mga.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/mga.a: $(BUILTIN_C)
	ar r $@ $^

