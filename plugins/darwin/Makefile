###############################################################################
# vlc (VideoLAN Client) Darwin module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_C = darwin.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../lib/darwin.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_DARWIN)

../../lib/darwin.a: $(BUILTIN_C)
	ar r $@ $^

