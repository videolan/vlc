###############################################################################
# vlc (VideoLAN Client) chroma conversion modules makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_YUV2RGB8 = yuv2rgb8.o #common.o

BUILTIN_YUV2RGB8 = $(PLUGIN_YUV2RGB8:%.o=BUILTIN_%.o)

PLUGIN_C = $(PLUGIN_YUV2RGB8)
BUILTIN_C = $(BUILTIN_YUV2RGB8)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../yuv2rgb8.so: $(PLUGIN_YUV2RGB8)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../yuv2rgb8.a: $(BUILTIN_YUV2RGB8)
	ar r $@ $^
	$(RANLIB) $@

