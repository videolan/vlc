###############################################################################
# vlc (VideoLAN Client) idct module makefile
# (c)2001 VideoLAN
###############################################################################

-include ../../Makefile.modules

#
# Objects
#

PLUGIN_IDCT = idct.o
PLUGIN_IDCTCLASSIC = idctclassic.o
PLUGIN_IDCTMMX = idctmmx.o
PLUGIN_IDCTMMXEXT = idctmmxext.o
PLUGIN_IDCTALTIVEC = idctaltivec.o
PLUGIN_IDCTCOMMON = idct_common.o

BUILTIN_IDCT = $(PLUGIN_IDCT:%.o=BUILTIN_IDCT_%.o) \
		$(PLUGIN_IDCTCOMMON:%.o=BUILTIN_IDCT_%.o)
BUILTIN_IDCTCLASSIC = $(PLUGIN_IDCTCLASSIC:%.o=BUILTIN_IDCTCLASSIC_%.o) \
		$(PLUGIN_IDCTCOMMON:%.o=BUILTIN_IDCTCLASSIC_%.o)
BUILTIN_IDCTMMX = $(PLUGIN_IDCTMMX:%.o=BUILTIN_IDCTMMX_%.o) \
		$(PLUGIN_IDCTCOMMON:%.o=BUILTIN_IDCTMMX_%.o)
BUILTIN_IDCTMMXEXT = $(PLUGIN_IDCTMMXEXT:%.o=BUILTIN_IDCTMMXEXT_%.o) \
		$(PLUGIN_IDCTCOMMON:%.o=BUILTIN_IDCTMMXEXT_%.o)
BUILTIN_IDCTALTIVEC = $(PLUGIN_IDCTALTIVEC:%.o=BUILTIN_IDCTALTIVEC_%.o) \
		$(PLUGIN_IDCTCOMMON:%.o=BUILTIN_IDCTALTIVEC_%.o)

PLUGIN_C = $(PLUGIN_IDCT) $(PLUGIN_IDCTCLASSIC) $(PLUGIN_IDCTMMX) $(PLUGIN_IDCTMMXEXT) $(PLUGIN_IDCTCOMMON)
ALL_OBJ = $(PLUGIN_C) $(PLUGIN_IDCTALTIVEC) $(BUILTIN_IDCT) $(BUILTIN_IDCTCLASSIC) $(BUILTIN_IDCTMMX) $(BUILTIN_IDCTMMXEXT) $(BUILTIN_IDCTALTIVEC)

#
# Virtual targets
#

include ../../Makefile.common

$(PLUGIN_IDCTALTIVEC): %.o: .dep/%.d
$(PLUGIN_IDCTALTIVEC): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -faltivec -c -o $@ $<

$(BUILTIN_IDCT): BUILTIN_IDCT_%.o: .dep/%.d
$(BUILTIN_IDCT): BUILTIN_IDCT_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=idct -c -o $@ $<

$(BUILTIN_IDCTCLASSIC): BUILTIN_IDCTCLASSIC_%.o: .dep/%.d
$(BUILTIN_IDCTCLASSIC): BUILTIN_IDCTCLASSIC_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=idctclassic -c -o $@ $<

$(BUILTIN_IDCTMMX): BUILTIN_IDCTMMX_%.o: .dep/%.d
$(BUILTIN_IDCTMMX): BUILTIN_IDCTMMX_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=idctmmx -c -o $@ $<

$(BUILTIN_IDCTMMXEXT): BUILTIN_IDCTMMXEXT_%.o: .dep/%.d
$(BUILTIN_IDCTMMXEXT): BUILTIN_IDCTMMXEXT_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=idctmmxext -c -o $@ $<

$(BUILTIN_IDCTALTIVEC): BUILTIN_IDCTALTIVEC_%.o: .dep/%.d
$(BUILTIN_IDCTALTIVEC): BUILTIN_IDCTALTIVEC_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=idctaltivec -c -o $@ $<

#
# Real targets
#

../../lib/idct.so: $(PLUGIN_IDCT) $(PLUGIN_IDCTCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/idct.a: $(BUILTIN_IDCT)
	ar r $@ $^

../../lib/idctclassic.so: $(PLUGIN_IDCTCLASSIC) $(PLUGIN_IDCTCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/idctclassic.a: $(BUILTIN_IDCTCLASSIC)
	ar r $@ $^

../../lib/idctmmx.so: $(PLUGIN_IDCTMMX) $(PLUGIN_IDCTCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/idctmmx.a: $(BUILTIN_IDCTMMX)
	ar r $@ $^

../../lib/idctmmxext.so: $(PLUGIN_IDCTMMXEXT) $(PLUGIN_IDCTCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/idctmmxext.a: $(BUILTIN_IDCTMMXEXT)
	ar r $@ $^

../../lib/idctaltivec.so: $(PLUGIN_IDCTALTIVEC) $(PLUGIN_IDCTCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_IDCTALTIVEC)

../../lib/idctaltivec.a: $(BUILTIN_IDCTALTIVEC)
	ar r $@ $^

