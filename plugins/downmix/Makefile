###############################################################################
# vlc (VideoLAN Client) downmix module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_DOWNMIX = downmix.o ac3_downmix_c.o
PLUGIN_DOWNMIXSSE = downmixsse.o ac3_downmix_sse.o
PLUGIN_DOWNMIX3DN = downmix3dn.o ac3_downmix_3dn.o

BUILTIN_DOWNMIX = $(PLUGIN_DOWNMIX:%.o=BUILTIN_DOWNMIX_%.o)
BUILTIN_DOWNMIXSSE = $(PLUGIN_DOWNMIXSSE:%.o=BUILTIN_DOWNMIXSSE_%.o)
BUILTIN_DOWNMIX3DN = $(PLUGIN_DOWNMIX3DN:%.o=BUILTIN_DOWNMIX3DN_%.o)

PLUGIN_C = $(PLUGIN_DOWNMIX) $(PLUGIN_DOWNMIXSSE) $(PLUGIN_DOWNMIX3DN)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_DOWNMIX) $(BUILTIN_DOWNMIXSSE) $(BUILTIN_DOWNMIX3DN)

#
# Virtual targets
#

include ../../Makefile.modules

$(BUILTIN_DOWNMIX): BUILTIN_DOWNMIX_%.o: .dep/%.d
$(BUILTIN_DOWNMIX): BUILTIN_DOWNMIX_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=downmix -c -o $@ $<

$(BUILTIN_DOWNMIXSSE): BUILTIN_DOWNMIXSSE_%.o: .dep/%.d
$(BUILTIN_DOWNMIXSSE): BUILTIN_DOWNMIXSSE_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=downmixsse -c -o $@ $<

$(BUILTIN_DOWNMIX3DN): BUILTIN_DOWNMIX3DN_%.o: .dep/%.d
$(BUILTIN_DOWNMIX3DN): BUILTIN_DOWNMIX3DN_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=downmix3dn -c -o $@ $<

#
# Real targets
#

../downmix.so: $(PLUGIN_DOWNMIX)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../downmix.a: $(BUILTIN_DOWNMIX)
	ar r $@ $^
	$(RANLIB) $@

../downmixsse.so: $(PLUGIN_DOWNMIXSSE)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../downmixsse.a: $(BUILTIN_DOWNMIXSSE)
	ar r $@ $^
	$(RANLIB) $@

../downmix3dn.so: $(PLUGIN_DOWNMIX3DN)
	$(CC) -o $@ $^ $(PLCFLAGS) 

../downmix3dn.a: $(BUILTIN_DOWNMIX3DN)
	ar r $@ $^
	$(RANLIB) $@

