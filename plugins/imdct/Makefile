###############################################################################
# vlc (VideoLAN Client) imdct module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_IMDCT = imdct.o ac3_imdct_c.o ac3_srfft_c.o
PLUGIN_IMDCTSSE = imdctsse.o ac3_imdct_sse.o ac3_srfft_sse.o
PLUGIN_IMDCT3DN = imdct3dn.o ac3_imdct_3dn.o ac3_srfft_3dn.o
PLUGIN_IMDCTCOMMON = ac3_imdct_common.o

BUILTIN_IMDCT = $(PLUGIN_IMDCT:%.o=BUILTIN_IMDCT_%.o) \
		$(PLUGIN_IMDCTCOMMON:%.o=BUILTIN_IMDCT_%.o)
BUILTIN_IMDCTSSE = $(PLUGIN_IMDCTSSE:%.o=BUILTIN_IMDCTSSE_%.o) \
		$(PLUGIN_IMDCTCOMMON:%.o=BUILTIN_IMDCTSSE_%.o)
BUILTIN_IMDCT3DN = $(PLUGIN_IMDCT3DN:%.o=BUILTIN_IMDCT3DN_%.o) \
		$(PLUGIN_IMDCTCOMMON:%.o=BUILTIN_IMDCT3DN_%.o)

PLUGIN_C = $(PLUGIN_IMDCT) $(PLUGIN_IMDCTSSE) $(PLUGIN_IMDCT3DN) $(PLUGIN_IMDCTCOMMON)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_IMDCT) $(BUILTIN_IMDCTSSE) $(BUILTIN_IMDCT3DN)

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

$(BUILTIN_IMDCT3DN): BUILTIN_IMDCT3DN_%.o: .dep/%.d
$(BUILTIN_IMDCT3DN): BUILTIN_IMDCT3DN_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=imdct3dn -c -o $@ $<

#
# Real targets
#

../imdct.so: $(PLUGIN_IMDCT) $(PLUGIN_IMDCTCOMMON) $(LIB_IMDCT)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../imdct.a: $(BUILTIN_IMDCT)
	ar r $@ $^
	$(RANLIB) $@

../imdctsse.so: $(PLUGIN_IMDCTSSE) $(PLUGIN_IMDCTCOMMON) $(LIB_IMDCTSSE)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../imdctsse.a: $(BUILTIN_IMDCTSSE)
	ar r $@ $^
	$(RANLIB) $@

../imdct3dn.so: $(PLUGIN_IMDCT3DN) $(PLUGIN_IMDCTCOMMON) $(LIB_IMDCT3DN)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../imdct3dn.a: $(BUILTIN_IMDCT3DN)
	ar r $@ $^
	$(RANLIB) $@

