###############################################################################
# vlc (VideoLAN Client) DirectX module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_C = directx.o vout_directx.o aout_directx.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../lib/directx.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_DIRECTX)

../../lib/directx.a: $(BUILTIN_C)
	ar r $@ $^
	$(RANLIB) $@

