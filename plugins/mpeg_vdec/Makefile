###############################################################################
# vlc (VideoLAN Client) MPEG audio decoder module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#
PLUGIN_C = video_parser.o vpar_headers.o vpar_blocks.o vpar_synchro.o vpar_pool.o video_decoder.o
BUILTIN_C = $(PLUGIN_C:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)
#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../mpeg_vdec.so: $(PLUGIN_C)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../mpeg_vdec.a: $(BUILTIN_C)
	ar r $@ $^
	$(RANLIB) $@

