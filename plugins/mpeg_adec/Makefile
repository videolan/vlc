###############################################################################
# vlc (VideoLAN Client) MPEG audio decoder module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#
PLUGIN_C = mpeg_adec.o adec_layer1.o adec_layer2.o adec_math.o mpeg_adec_generic.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)
#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../mpeg_adec.so: $(PLUGIN_C)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../mpeg_adec.a: $(BUILTIN_C)
	ar r $@ $^
	$(RANLIB) $@

