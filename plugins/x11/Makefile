###############################################################################
# vlc (VideoLAN Client) x11 module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_X11 = x11.o vout_x11.o
PLUGIN_XVIDEO = xvideo.o vout_xvideo.o
PLUGIN_COMMON = vout_common.o

BUILTIN_X11 = $(PLUGIN_X11:%.o=BUILTIN_X11_%.o) \
              $(PLUGIN_COMMON:%.o=BUILTIN_X11_%.o)
BUILTIN_XVIDEO = $(PLUGIN_XVIDEO:%.o=BUILTIN_XVIDEO_%.o) \
                 $(PLUGIN_COMMON:%.o=BUILTIN_XVIDEO_%.o)

ALL_OBJ = $(PLUGIN_X11) $(PLUGIN_XVIDEO) $(PLUGIN_COMMON) \
	  $(PLUGIN_COMMON:%.o=%_xvideo.o) $(BUILTIN_X11) $(BUILTIN_XVIDEO)

#
# Virtual targets
#

include ../../Makefile.modules

# X11
$(PLUGIN_X11) $(PLUGIN_COMMON): %.o: .dep/%.d
$(PLUGIN_X11) $(PLUGIN_COMMON): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) $(CFLAGS_X11) -DPLUGIN -DMODULE_NAME=x11 -c -o $@ $<

$(BUILTIN_X11): BUILTIN_X11_%.o: .dep/%.d
$(BUILTIN_X11): BUILTIN_X11_%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_X11) -DBUILTIN -DMODULE_NAME=x11 -c -o $@ $<

# XVideo
$(PLUGIN_XVIDEO): %.o: .dep/%.d
$(PLUGIN_XVIDEO): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) $(CFLAGS_XVIDEO) -DPLUGIN -DMODULE_NAME=xvideo -c -o $@ $<

$(PLUGIN_COMMON:%.o=%_xvideo.o): %_xvideo.o: .dep/%.d
$(PLUGIN_COMMON:%.o=%_xvideo.o): %_xvideo.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) $(CFLAGS_XVIDEO) -DPLUGIN -DMODULE_NAME=xvideo -c -o $@ $<

$(BUILTIN_XVIDEO): BUILTIN_XVIDEO_%.o: .dep/%.d
$(BUILTIN_XVIDEO): BUILTIN_XVIDEO_%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_XVIDEO) -DBUILTIN -DMODULE_NAME=xvideo -c -o $@ $<

#
# Real targets
#

../x11.so: $(PLUGIN_X11) $(PLUGIN_COMMON)
	$(CC) -o $@ $^ $(PLCFLAGS) $(LIB_X11)

../x11.a: $(BUILTIN_X11)
	ar r $@ $^
	$(RANLIB) $@

../xvideo.so: $(PLUGIN_XVIDEO) $(PLUGIN_COMMON:%.o=%_xvideo.o)
	$(CC) -o $@ $^ $(PLCFLAGS) $(LIB_XVIDEO)

../xvideo.a: $(BUILTIN_XVIDEO)
	ar r $@ $^
	$(RANLIB) $@

