###############################################################################
# vlc (VideoLAN Client) yuv module makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_YUV = yuv.o video_yuv.o transforms_yuv.o
PLUGIN_YUVMMX = yuvmmx.o video_yuvmmx.o transforms_yuvmmx.o

BUILTIN_YUV = $(PLUGIN_YUV:%.o=%-BUILTIN.o)
BUILTIN_YUVMMX = $(PLUGIN_YUVMMX:%.o=%-BUILTIN.o)

STD_PLUGIN_OBJ = $(PLUGIN_YUV) $(PLUGIN_YUVMMX)
STD_BUILTIN_OBJ = $(BUILTIN_YUV) $(BUILTIN_YUVMMX)
ALL_OBJ = $(STD_PLUGIN_OBJ) $(STD_BUILTIN_OBJ)

objects := $(ALL_OBJ)
cdependancies := $(objects:%.o=.dep/%.d)

export

#
# Virtual targets
#
all:

clean:
	rm -f $(ALL_OBJ)
	rm -f *.o *.moc *.bak *.builtin
	rm -rf .dep

FORCE:

$(cdependancies): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(ALL_OBJ): %.o: ../../Makefile.dep Makefile

$(STD_PLUGIN_OBJ): %.o: .dep/%.d
$(STD_PLUGIN_OBJ): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

$(STD_BUILTIN_OBJ): %-BUILTIN.o: .dep/%.d
$(STD_BUILTIN_OBJ): %-BUILTIN.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -c -o $@ $<

#
# Real targets
#
../../lib/yuv.so: $(PLUGIN_YUV)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/yuv.a: $(BUILTIN_YUV)
	ar r $@ $^

../../lib/yuvmmx.so: $(PLUGIN_YUVMMX)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/yuvmmx.a: $(BUILTIN_YUVMMX)
	ar r $@ $^

