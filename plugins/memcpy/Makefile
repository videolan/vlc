###############################################################################
# vlc (VideoLAN Client) memcpy module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_MEMCPY = memcpy.o
PLUGIN_MEMCPYMMX = memcpymmx.o
PLUGIN_MEMCPYMMXEXT = memcpymmxext.o
PLUGIN_MEMCPY3DN = memcpy3dn.o

BUILTIN_MEMCPY = $(PLUGIN_MEMCPY:%.o=BUILTIN_%.o)
BUILTIN_MEMCPYMMX = $(PLUGIN_MEMCPYMMX:%.o=BUILTIN_%.o)
BUILTIN_MEMCPYMMXEXT = $(PLUGIN_MEMCPYMMXEXT:%.o=BUILTIN_%.o)
BUILTIN_MEMCPY3DN = $(PLUGIN_MEMCPY3DN:%.o=BUILTIN_%.o)

PLUGIN_C = $(PLUGIN_MEMCPY) $(PLUGIN_MEMCPYMMX) $(PLUGIN_MEMCPYMMXEXT) $(PLUGIN_MEMCPY3DN)
BUILTIN_C = $(BUILTIN_MEMCPY) $(BUILTIN_MEMCPYMMX) $(BUILTIN_MEMCPYMMXEXT) $(BUILTIN_MEMCPY3DN)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../memcpy.so: $(PLUGIN_MEMCPY)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../memcpy.a: $(BUILTIN_MEMCPY)
	ar r $@ $^
	$(RANLIB) $@

../memcpymmx.so: $(PLUGIN_MEMCPYMMX)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../memcpymmx.a: $(BUILTIN_MEMCPYMMX)
	ar r $@ $^
	$(RANLIB) $@

../memcpymmxext.so: $(PLUGIN_MEMCPYMMXEXT)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../memcpymmxext.a: $(BUILTIN_MEMCPYMMXEXT)
	ar r $@ $^
	$(RANLIB) $@

../memcpy3dn.so: $(PLUGIN_MEMCPY3DN)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../memcpy3dn.a: $(BUILTIN_MEMCPY3DN)
	ar r $@ $^
	$(RANLIB) $@

