###############################################################################
# vlc (VideoLAN Client) imdct module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_IMDCT = imdct.o ac3_imdct_c.o ac3_srfft_c.o
PLUGIN_IMDCTSSE = imdctsse.o ac3_imdct_sse.o ac3_srfft_sse.o
PLUGIN_IMDCTCOMMON = ac3_imdct_common.o

BUILTIN_IMDCT = $(PLUGIN_IMDCT:%.o=BUILTIN_IMDCT_%.o) \
		$(PLUGIN_IMDCTCOMMON:%.o=BUILTIN_IMDCT_%.o)
BUILTIN_IMDCTSSE = $(PLUGIN_IMDCTSSE:%.o=BUILTIN_IMDCTSSE_%.o) \
		$(PLUGIN_IMDCTCOMMON:%.o=BUILTIN_IMDCTSSE_%.o)

PLUGIN_C = $(PLUGIN_IMDCT) $(PLUGIN_IMDCTSSE) $(PLUGIN_IMDCTCOMMON)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_IMDCT) $(BUILTIN_IMDCTSSE)

#
# Virtual targets
#

include ../../Makefile.modules

$(BUILTIN_IMDCT): BUILTIN_IMDCT_%.o: .dep/%.d
$(BUILTIN_IMDCT): BUILTIN_IMDCT_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=imdct -c -o $@ $<

$(BUILTIN_IMDCTSSE): BUILTIN_IMDCTSSE_%.o: .dep/%.d
$(BUILTIN_IMDCTSSE): BUILTIN_IMDCTSSE_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=imdctsse -c -o $@ $<

#
# Real targets
#

../../lib/imdct.so: $(PLUGIN_IMDCT) $(PLUGIN_IMDCTCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/imdct.a: $(BUILTIN_IMDCT)
	ar r $@ $^
	$(RANLIB) $@

../../lib/imdctsse.so: $(PLUGIN_IMDCTSSE) $(PLUGIN_IMDCTCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/imdctsse.a: $(BUILTIN_IMDCTSSE)
	ar r $@ $^
	$(RANLIB) $@

