###############################################################################
# vlc (VideoLAN Client) image filter modules makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_BOB = bob.o
PLUGIN_BOB422 = bob422.o
PLUGIN_INVERT = invert.o
PLUGIN_WALL = wall.o

BUILTIN_BOB = $(PLUGIN_BOB:%.o=BUILTIN_%.o)
BUILTIN_BOB422 = $(PLUGIN_BOB422:%.o=BUILTIN_%.o)
BUILTIN_INVERT = $(PLUGIN_INVERT:%.o=BUILTIN_%.o)
BUILTIN_WALL = $(PLUGIN_WALL:%.o=BUILTIN_%.o)

PLUGIN_C = $(PLUGIN_BOB) $(PLUGIN_BOB422) $(PLUGIN_INVERT) $(PLUGIN_WALL)
BUILTIN_C = $(BUILTIN_BOB) $(BUILTIN_BOB422) $(BUILTIN_INVERT) $(BUILTIN_WALL)
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

../filter_bob422.so: $(PLUGIN_BOB422)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../filter_bob422.a: $(BUILTIN_BOB422)
	ar r $@ $^
	$(RANLIB) $@

../filter_invert.so: $(PLUGIN_INVERT)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../filter_invert.a: $(BUILTIN_INVERT)
	ar r $@ $^
	$(RANLIB) $@

../filter_wall.so: $(PLUGIN_WALL)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../filter_wall.a: $(BUILTIN_WALL)
	ar r $@ $^
	$(RANLIB) $@

