###############################################################################
# vlc (VideoLAN Client) idct module makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_IDCT = idct.o
PLUGIN_IDCTCLASSIC = idctclassic.o
PLUGIN_IDCTMMX = idctmmx.o
PLUGIN_IDCTMMXEXT = idctmmxext.o
PLUGIN_IDCTALTIVEC = idctaltivec.o
PLUGIN_IDCTCOMMON = idct_common.o

BUILTIN_IDCT = $(PLUGIN_IDCT:%.o=%-BUILTIN-IDCT.o) \
		$(PLUGIN_IDCTCOMMON:%.o=%-BUILTIN-IDCT.o)
BUILTIN_IDCTCLASSIC = $(PLUGIN_IDCTCLASSIC:%.o=%-BUILTIN-IDCTCLASSIC.o) \
		$(PLUGIN_IDCTCOMMON:%.o=%-BUILTIN-IDCTCLASSIC.o)
BUILTIN_IDCTMMX = $(PLUGIN_IDCTMMX:%.o=%-BUILTIN-IDCTMMX.o) \
		$(PLUGIN_IDCTCOMMON:%.o=%-BUILTIN-IDCTMMX.o)
BUILTIN_IDCTMMXEXT = $(PLUGIN_IDCTMMXEXT:%.o=%-BUILTIN-IDCTMMXEXT.o) \
		$(PLUGIN_IDCTCOMMON:%.o=%-BUILTIN-IDCTMMXEXT.o)
BUILTIN_IDCTALTIVEC = $(PLUGIN_IDCTALTIVEC:%.o=%-BUILTIN-IDCTALTIVEC.o) \
		$(PLUGIN_IDCTCOMMON:%.o=%-BUILTIN-IDCTALTIVEC.o)

STD_PLUGIN_OBJ = $(PLUGIN_IDCT) $(PLUGIN_IDCTCLASSIC) $(PLUGIN_IDCTMMX) $(PLUGIN_IDCTMMXEXT) $(PLUGIN_IDCTCOMMON)
ALL_OBJ = $(STD_PLUGIN_OBJ) $(PLUGIN_IDCTALTIVEC) $(BUILTIN_IDCT) $(BUILTIN_IDCTCLASSIC) $(BUILTIN_IDCTMMX) $(BUILTIN_IDCTMMXEXT) $(BUILTIN_IDCTALTIVEC)

objects := $(ALL_OBJ)
cdependancies := $(objects:%.o=.dep/%.d)

export

#
# Virtual targets
#
all:

clean:
	rm -f $(ALL_OBJ)
	rm -f *.o *.moc *.bak *.so *.a *.builtin
	rm -rf .dep

FORCE:

$(cdependancies): %.d: FORCE
	@$(MAKE) -s --no-print-directory -f ../../Makefile.dep $@

$(ALL_OBJ): %.o: ../../Makefile.dep

$(STD_PLUGIN_OBJ): %.o: .dep/%.d
$(STD_PLUGIN_OBJ): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -c -o $@ $<

$(PLUGIN_IDCTALTIVEC): %.o: .dep/%.d
$(PLUGIN_IDCTALTIVEC): %.o: %.c
	$(CC) $(CFLAGS) $(PCFLAGS) -faltivec -c -o $@ $<

$(BUILTIN_IDCT): %-BUILTIN-IDCT.o: .dep/%.d
$(BUILTIN_IDCT): %-BUILTIN-IDCT.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=idct -c -o $@ $<

$(BUILTIN_IDCTCLASSIC): %-BUILTIN-IDCTCLASSIC.o: .dep/%.d
$(BUILTIN_IDCTCLASSIC): %-BUILTIN-IDCTCLASSIC.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=idctclassic -c -o $@ $<

$(BUILTIN_IDCTMMX): %-BUILTIN-IDCTMMX.o: .dep/%.d
$(BUILTIN_IDCTMMX): %-BUILTIN-IDCTMMX.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=idctmmx -c -o $@ $<

$(BUILTIN_IDCTMMXEXT): %-BUILTIN-IDCTMMXEXT.o: .dep/%.d
$(BUILTIN_IDCTMMXEXT): %-BUILTIN-IDCTMMXEXT.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=idctmmxext -c -o $@ $<

$(BUILTIN_IDCTALTIVEC): %-BUILTIN-IDCTALTIVEC.o: .dep/%.d
$(BUILTIN_IDCTALTIVEC): %-BUILTIN-IDCTALTIVEC.o: %.c
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
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -framework vecLib

../../lib/idctaltivec.a: $(BUILTIN_IDCTALTIVEC)
	ar r $@ $^

