###############################################################################
# vlc (VideoLAN Client) OSS /dev/dsp module Makefile
# (c)2001 VideoLAN
###############################################################################

-include ../../Makefile.modules

#
# Objects
#

PLUGIN_C = dsp.o aout_dsp.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.common

#
# Real targets
#

../../lib/dsp.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/dsp.a: $(BUILTIN_C)
	ar r $@ $^

