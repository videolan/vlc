###############################################################################
# vlc (VideoLAN Client) yuv module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_YUV = yuv.o video_yuv.o transforms_yuv.o
PLUGIN_YUVMMX = yuvmmx.o video_yuvmmx.o transforms_yuvmmx.o

BUILTIN_YUV = $(PLUGIN_YUV:%.o=BUILTIN_%.o)
BUILTIN_YUVMMX = $(PLUGIN_YUVMMX:%.o=BUILTIN_%.o)

PLUGIN_C = $(PLUGIN_YUV) $(PLUGIN_YUVMMX)
BUILTIN_C = $(BUILTIN_YUV) $(BUILTIN_YUVMMX)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../lib/yuv.so: $(PLUGIN_YUV)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_YUV)

../../lib/yuv.a: $(BUILTIN_YUV)
	ar r $@ $^

../../lib/yuvmmx.so: $(PLUGIN_YUVMMX)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/yuvmmx.a: $(BUILTIN_YUVMMX)
	ar r $@ $^

