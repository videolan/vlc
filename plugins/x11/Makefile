###############################################################################
# vlc (VideoLAN Client) x11 module Makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_X11 = x11.o vout_x11.o
PLUGIN_XVIDEO = xvideo.o vout_xvideo.o
BUILTIN_X11 = $(PLUGIN_X11:%.o=%-BUILTIN.o)
BUILTIN_XVIDEO = $(PLUGIN_XVIDEO:%.o=%-BUILTIN.o)

STD_PLUGIN_OBJ = $(PLUGIN_X11) $(PLUGIN_XVIDEO)
STD_BUILTIN_OBJ = $(BUILTIN_X11) $(BUILTIN_XVIDEO)

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
../../lib/x11.so: $(PLUGIN_X11)
ifeq ($(SYS),nto-qnx)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -L/usr/X11R6/lib -lX11 -lXext -lsocket
else
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -L/usr/X11R6/lib -lX11 -lXext
endif

../../lib/x11.a: $(BUILTIN_X11)
	ar r $@ $^

../../lib/xvideo.so: $(PLUGIN_XVIDEO)
ifeq ($(SYS),nto-qnx)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -L/usr/X11R6/lib -lX11 -lXext -lXv -lsocket
else
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -L/usr/X11R6/lib -lX11 -lXext -lXv
endif

../../lib/xvideo.a: $(BUILTIN_XVIDEO)
	ar r $@ $^

