###############################################################################
# vlc (VideoLAN Client) mpeg module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_ES = es.o input_es.o
PLUGIN_PS = ps.o input_ps.o
PLUGIN_TS = ts.o input_ts.o
BUILTIN_ES = $(PLUGIN_ES:%.o=BUILTIN_%.o)
BUILTIN_PS = $(PLUGIN_PS:%.o=BUILTIN_%.o)
BUILTIN_TS = $(PLUGIN_TS:%.o=BUILTIN_%.o)

PLUGIN_C = $(PLUGIN_ES) $(PLUGIN_PS) $(PLUGIN_TS)
BUILTIN_C = $(BUILTIN_ES) $(BUILTIN_PS) $(BUILTIN_TS)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../lib/es.so: $(PLUGIN_ES)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/es.a: $(BUILTIN_ES)
	ar r $@ $^

../../lib/ps.so: $(PLUGIN_PS)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/ps.a: $(BUILTIN_PS)
	ar r $@ $^

../../lib/ts.so: $(PLUGIN_TS)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_TS)

../../lib/ts.a: $(BUILTIN_TS)
	ar r $@ $^

