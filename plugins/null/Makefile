###############################################################################
# vlc (VideoLAN Client) null module makefile
# (c)2001 VideoLAN
###############################################################################

-include ../../Makefile.modules

#
# Objects
#

PLUGIN_C = null.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.common

#
# Real targets
#

../../lib/null.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/null.a: $(BUILTIN_C)
	ar r $@ $^

