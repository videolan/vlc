###############################################################################
# vlc (VideoLAN Client) chroma conversion modules makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_YV12_RGB8 = yv12_rgb8.o

BUILTIN_YV12_RGB8 = $(PLUGIN_YV12_RGB8:%.o=BUILTIN_%.o)

PLUGIN_C = $(PLUGIN_YV12_RGB8)
BUILTIN_C = $(BUILTIN_YV12_RGB8)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../chroma_yv12_rgb8.so: $(PLUGIN_YV12_RGB8)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../chroma_yv12_rgb8.a: $(BUILTIN_YV12_RGB8)
	ar r $@ $^
	$(RANLIB) $@

