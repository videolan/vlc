###############################################################################
# vlc (VideoLAN Client) MAD audio decoder module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#
PLUGIN_MAD = mad_adec.o mad_libmad.o
BUILTIN_MAD = $(PLUGIN_MAD:%.o=BUILTIN_%.o)

ALL_OBJ = $(PLUGIN_MAD) $(BUILTIN_MAD)
#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_MAD): %.o: .dep/%.d
$(PLUGIN_MAD): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) $(CFLAGS_MAD) -DPLUGIN -c -o $@ $<

$(BUILTIN_MAD): BUILTIN_%.o: .dep/%.d
$(BUILTIN_MAD): BUILTIN_%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_MAD) -DBUILTIN -c -o $@ $<

#
# Real targets
#

../mad.so: $(PLUGIN_MAD)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_MAD)

../mad.a: $(BUILTIN_MAD)
	ar r $@ $^
	$(RANLIB) $@

