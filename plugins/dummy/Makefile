###############################################################################
# vlc (VideoLAN Client) dummy module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_C = dummy.o aout_dummy.o vout_dummy.o intf_dummy.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../lib/dummy.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/dummy.a: $(BUILTIN_C)
	ar r $@ $^
	$(RANLIB) $@

