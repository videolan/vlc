###############################################################################
# vlc (VideoLAN Client) idct module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_IDCT = idct.o
PLUGIN_IDCTCLASSIC = idctclassic.o
PLUGIN_IDCTMMX = idctmmx.o
PLUGIN_IDCTMMXEXT = idctmmxext.o
PLUGIN_IDCTALTIVEC = idctaltivec.o
PLUGIN_IDCTCOMMON = vdec_idct.o
PLUGIN_VDECBLOCK_C = vdec_block_c.o
PLUGIN_VDECBLOCK_MMX = vdec_block_mmx.o

BUILTIN_IDCT = $(PLUGIN_IDCT:%.o=BUILTIN_IDCT_%.o) \
		$(PLUGIN_IDCTCOMMON:%.o=BUILTIN_IDCT_%.o) \
		$(PLUGIN_VDECBLOCK_C:%.o=BUILTIN_IDCT_%.o)
BUILTIN_IDCTCLASSIC = $(PLUGIN_IDCTCLASSIC:%.o=BUILTIN_IDCTCLASSIC_%.o) \
		$(PLUGIN_IDCTCOMMON:%.o=BUILTIN_IDCTCLASSIC_%.o) \
		$(PLUGIN_VDECBLOCK_C:%.o=BUILTIN_IDCTCLASSIC_%.o)
BUILTIN_IDCTMMX = $(PLUGIN_IDCTMMX:%.o=BUILTIN_IDCTMMX_%.o) \
		$(PLUGIN_IDCTCOMMON:%.o=BUILTIN_IDCTMMX_%.o) \
		$(PLUGIN_VDECBLOCK_MMX:%.o=BUILTIN_IDCTMMX_%.o)
BUILTIN_IDCTMMXEXT = $(PLUGIN_IDCTMMXEXT:%.o=BUILTIN_IDCTMMXEXT_%.o) \
		$(PLUGIN_IDCTCOMMON:%.o=BUILTIN_IDCTMMXEXT_%.o) \
		$(PLUGIN_VDECBLOCK_MMX:%.o=BUILTIN_IDCTMMXEXT_%.o)
BUILTIN_IDCTALTIVEC = $(PLUGIN_IDCTALTIVEC:%.o=BUILTIN_IDCTALTIVEC_%.o) \
		$(PLUGIN_IDCTCOMMON:%.o=BUILTIN_IDCTALTIVEC_%.o) \
		$(PLUGIN_VDECBLOCK_C:%.o=BUILTIN_IDCTALTIVEC_%.o)

PLUGIN_C = $(PLUGIN_IDCT) $(PLUGIN_IDCTCLASSIC) $(PLUGIN_IDCTMMX) $(PLUGIN_IDCTMMXEXT) $(PLUGIN_IDCTCOMMON) $(PLUGIN_VDECBLOCK_C) $(PLUGIN_VDECBLOCK_MMX)
ALL_OBJ = $(PLUGIN_C) $(PLUGIN_IDCTALTIVEC) $(BUILTIN_IDCT) $(BUILTIN_IDCTCLASSIC) $(BUILTIN_IDCTMMX) $(BUILTIN_IDCTMMXEXT) $(BUILTIN_IDCTALTIVEC)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_IDCTALTIVEC): %.o: .dep/%.d
$(PLUGIN_IDCTALTIVEC): %.o: %.c
	$(CC) $(CFLAGS) -DPLUGIN $(PCFLAGS) -faltivec -c -o $@ $<

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

../../plugins/idct.so: $(PLUGIN_IDCT) $(PLUGIN_IDCTCOMMON) $(PLUGIN_VDECBLOCK_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../plugins/idct.a: $(BUILTIN_IDCT)
	ar r $@ $^
	$(RANLIB) $@

../../plugins/idctclassic.so: $(PLUGIN_IDCTCLASSIC) $(PLUGIN_IDCTCOMMON) $(PLUGIN_VDECBLOCK_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../plugins/idctclassic.a: $(BUILTIN_IDCTCLASSIC)
	ar r $@ $^
	$(RANLIB) $@

../../plugins/idctmmx.so: $(PLUGIN_IDCTMMX) $(PLUGIN_IDCTCOMMON) $(PLUGIN_VDECBLOCK_MMX)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../plugins/idctmmx.a: $(BUILTIN_IDCTMMX)
	ar r $@ $^
	$(RANLIB) $@

../../plugins/idctmmxext.so: $(PLUGIN_IDCTMMXEXT) $(PLUGIN_IDCTCOMMON) $(PLUGIN_VDECBLOCK_MMX)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../plugins/idctmmxext.a: $(BUILTIN_IDCTMMXEXT)
	ar r $@ $^
	$(RANLIB) $@

../../plugins/idctaltivec.so: $(PLUGIN_IDCTALTIVEC) $(PLUGIN_IDCTCOMMON) $(PLUGIN_VDECBLOCK_C)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_IDCTALTIVEC)

../../plugins/idctaltivec.a: $(BUILTIN_IDCTALTIVEC)
	ar r $@ $^
	$(RANLIB) $@

