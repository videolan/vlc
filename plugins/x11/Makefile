###############################################################################
# vlc (VideoLAN Client) x11 module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_X11 = x11.o vout_x11.o
PLUGIN_XVIDEO = xvideo.o vout_xvideo.o
PLUGIN_XCOMMON = vout_common.o

BUILTIN_X11 = $(PLUGIN_X11:%.o=BUILTIN_X11_%.o) \
              $(PLUGIN_XCOMMON:%.o=BUILTIN_X11_%.o)
BUILTIN_XVIDEO = $(PLUGIN_XVIDEO:%.o=BUILTIN_XVIDEO_%.o) \
                 $(PLUGIN_XCOMMON:%.o=BUILTIN_XVIDEO_%.o)

PLUGIN_C = $(PLUGIN_X11) $(PLUGIN_XVIDEO) $(PLUGIN_XCOMMON)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_X11) $(BUILTIN_XVIDEO)

#
# Virtual targets
#

include ../../Makefile.modules

$(BUILTIN_X11): BUILTIN_X11_%.o: .dep/%.d
$(BUILTIN_X11): BUILTIN_X11_%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_X11) -DBUILTIN -DMODULE_NAME=x11 -c -o $@ $<

$(BUILTIN_XVIDEO): BUILTIN_XVIDEO_%.o: .dep/%.d
$(BUILTIN_XVIDEO): BUILTIN_XVIDEO_%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_XVIDEO) -DBUILTIN -DMODULE_NAME=xvideo -c -o $@ $<

#
# Real targets
#

../x11.so: $(PLUGIN_X11)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_X11)

../x11.a: $(BUILTIN_X11)
	ar r $@ $^
	$(RANLIB) $@

../xvideo.so: $(PLUGIN_XVIDEO)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_XVIDEO)

../xvideo.a: $(BUILTIN_XVIDEO)
	ar r $@ $^
	$(RANLIB) $@

