###############################################################################
# vlc (VideoLAN Client) x11 module Makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_X11 = x11.o vout_x11.o
PLUGIN_XVIDEO = xvideo.o vout_xvideo.o
BUILTIN_X11 = $(PLUGIN_X11:%.o=BUILTIN_%.o)
BUILTIN_XVIDEO = $(PLUGIN_XVIDEO:%.o=BUILTIN_%.o)

PLUGIN_C = $(PLUGIN_X11) $(PLUGIN_XVIDEO)
BUILTIN_C = $(BUILTIN_X11) $(BUILTIN_XVIDEO)

ALL_OBJ = $(PLUGIN_C) $(BUILTIN_C)

#
# Special plugin modifications
#

CFLAGS += $(CFLAGS_X11)

#
# Virtual targets
#

include ../../Makefile.modules

#
# Real targets
#

../../plugins/x11.so: $(PLUGIN_X11)
ifeq ($(SYS),nto-qnx)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_X11) -lsocket
else
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_X11)
endif

../../plugins/x11.a: $(BUILTIN_X11)
	ar r $@ $^
	$(RANLIB) $@

../../plugins/xvideo.so: $(PLUGIN_XVIDEO)
ifeq ($(SYS),nto-qnx)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_XVIDEO) -lsocket
else
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_XVIDEO)
endif

../../plugins/xvideo.a: $(BUILTIN_XVIDEO)
	ar r $@ $^
	$(RANLIB) $@

