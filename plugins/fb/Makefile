###############################################################################
# vlc (VideoLAN Client) fb module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_C = fb.o vout_fb.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../lib/fb.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/fb.a: $(BUILTIN_C)
	ar r $@ $^

