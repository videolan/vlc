###############################################################################
# vlc (VideoLAN Client) mpeg module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_ES = mpeg_es.o input_es.o
PLUGIN_PS = mpeg_ps.o input_ps.o
PLUGIN_TS = mpeg_ts.o input_ts.o
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

../mpeg_es.so: $(PLUGIN_ES)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../mpeg_es.a: $(BUILTIN_ES)
	ar r $@ $^
	$(RANLIB) $@

../mpeg_ps.so: $(PLUGIN_PS)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../mpeg_ps.a: $(BUILTIN_PS)
	ar r $@ $^
	$(RANLIB) $@

../mpeg_ts.so: $(PLUGIN_TS)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_TS)

../mpeg_ts.a: $(BUILTIN_TS)
	ar r $@ $^
	$(RANLIB) $@

