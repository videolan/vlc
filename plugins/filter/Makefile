###############################################################################
# vlc (VideoLAN Client) image filter modules makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_BOB = bob.o
PLUGIN_INVERT = invert.o

BUILTIN_BOB = $(PLUGIN_BOB:%.o=BUILTIN_%.o)
BUILTIN_INVERT = $(PLUGIN_INVERT:%.o=BUILTIN_%.o)

PLUGIN_C = $(PLUGIN_BOB) $(PLUGIN_INVERT)
BUILTIN_C = $(BUILTIN_BOB) $(BUILTIN_INVERT)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../filter_bob.so: $(PLUGIN_BOB)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../filter_bob.a: $(BUILTIN_BOB)
	ar r $@ $^
	$(RANLIB) $@

../filter_invert.so: $(PLUGIN_INVERT)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../filter_invert.a: $(BUILTIN_INVERT)
	ar r $@ $^
	$(RANLIB) $@

