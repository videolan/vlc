###############################################################################
# vlc (VideoLAN Client) macosx module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_MACOSX = macosx.o intf_main.o intf_controller.o aout_macosx.o vout_macosx.o intf_vlc_wrapper.o intf_qdview.o
BUILTIN_MACOSX = $(PLUGIN_MACOSX:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_MACOSX) $(BUILTIN_MACOSX)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_MACOSX): %.o: .dep/%.d
$(PLUGIN_MACOSX): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -ObjC -c -o $@ $<

$(BUILTIN_MACOSX): BUILTIN_%.o: .dep/%.d
$(BUILTIN_MACOSX): BUILTIN_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -ObjC -c -o $@ $<

#
# Real targets
#

../macosx.so: $(PLUGIN_MACOSX)
	$(CC) -o $@ $^ $(PLCFLAGS) $(LIB_MACOSX) -lobjc

../macosx.a: $(BUILTIN_MACOSX)
	ar r $@ $^
	$(RANLIB) $@

