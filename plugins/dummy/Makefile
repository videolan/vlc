###############################################################################
# vlc (VideoLAN Client) dummy and null module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_NULL = null.o
PLUGIN_DUMMY = dummy.o aout_dummy.o vout_dummy.o intf_dummy.o input_dummy.o
BUILTIN_NULL = $(PLUGIN_NULL:%.o=BUILTIN_%.o)
BUILTIN_DUMMY = $(PLUGIN_DUMMY:%.o=BUILTIN_%.o)

PLUGIN_C = $(PLUGIN_NULL) $(PLUGIN_DUMMY)
BUILTIN_C = $(BUILTIN_NULL) $(BUILTIN_DUMMY)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../plugins/null.so: $(PLUGIN_NULL)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../plugins/null.a: $(BUILTIN_NULL)
	ar r $@ $^
	$(RANLIB) $@

../../plugins/dummy.so: $(PLUGIN_DUMMY)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../plugins/dummy.a: $(BUILTIN_DUMMY)
	ar r $@ $^
	$(RANLIB) $@

