###############################################################################
# vlc (VideoLAN Client) image filter modules makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_BOB = bob.o
PLUGIN_TRANSFORM = transform.o
PLUGIN_INVERT = invert.o
PLUGIN_DISTORT = distort.o
PLUGIN_WALL = wall.o

BUILTIN_BOB = $(PLUGIN_BOB:%.o=BUILTIN_%.o)
BUILTIN_TRANSFORM = $(PLUGIN_TRANSFORM:%.o=BUILTIN_%.o)
BUILTIN_INVERT = $(PLUGIN_INVERT:%.o=BUILTIN_%.o)
BUILTIN_DISTORT = $(PLUGIN_DISTORT:%.o=BUILTIN_%.o)
BUILTIN_WALL = $(PLUGIN_WALL:%.o=BUILTIN_%.o)

PLUGIN_C = $(PLUGIN_BOB) $(PLUGIN_TRANSFORM) $(PLUGIN_INVERT) $(PLUGIN_DISTORT) $(PLUGIN_WALL)
BUILTIN_C = $(BUILTIN_BOB) $(BUILTIN_TRANSFORM) $(BUILTIN_INVERT) $(BUILTIN_DISTORT) $(BUILTIN_WALL)
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

../filter_transform.so: $(PLUGIN_TRANSFORM)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../filter_transform.a: $(BUILTIN_TRANSFORM)
	ar r $@ $^
	$(RANLIB) $@

../filter_invert.so: $(PLUGIN_INVERT)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../filter_invert.a: $(BUILTIN_INVERT)
	ar r $@ $^
	$(RANLIB) $@

../filter_distort.so: $(PLUGIN_DISTORT)
	$(CC) -o $@ $^ $(PLCFLAGS) $(LIB_FILTER_DISTORT)

../filter_distort.a: $(BUILTIN_DISTORT)
	ar r $@ $^
	$(RANLIB) $@

../filter_wall.so: $(PLUGIN_WALL)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../filter_wall.a: $(BUILTIN_WALL)
	ar r $@ $^
	$(RANLIB) $@

