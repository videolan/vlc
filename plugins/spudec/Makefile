###############################################################################
# vlc (VideoLAN Client) MPEG audio decoder module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#
PLUGIN_C = spu_decoder.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)
#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../spudec.so: $(PLUGIN_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../spudec.a: $(BUILTIN_C)
	ar r $@ $^
	$(RANLIB) $@

