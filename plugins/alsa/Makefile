###############################################################################
# vlc (VideoLAN Client) ALSA module Makefile
# (c)2001 VideoLAN
###############################################################################

-include ../../Makefile.modules

#
# Objects
#

PLUGIN_C = alsa.o aout_alsa.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.common

#
# Real targets
#

../../lib/alsa.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_ALSA)

../../lib/alsa.a: $(BUILTIN_C)
	ar r $@ $^

