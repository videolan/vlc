###############################################################################
# vlc (VideoLAN Client) motion module makefile
# (c)2001 VideoLAN
###############################################################################

include ../../Makefile.modules

###############################################################################
# Objects and files
###############################################################################
PLUGIN_MOTION = motion.o vdec_motion_inner.o
PLUGIN_MOTIONMMX = motionmmx.o vdec_motion_inner_mmx.o
PLUGIN_MOTIONMMXEXT = motionmmxext.o vdec_motion_inner_mmxext.o
PLUGIN_MOTIONCOMMON = vdec_motion_common.o

BUILTIN_MOTION = $(PLUGIN_MOTION:%.o=%-BUILTIN-MOTION.o) \
		$(PLUGIN_MOTIONCOMMON:%.o=%-BUILTIN-MOTION.o)
BUILTIN_MOTIONMMX = $(PLUGIN_MOTIONMMX:%.o=%-BUILTIN-MOTIONMMX.o) \
		$(PLUGIN_MOTIONCOMMON:%.o=%-BUILTIN-MOTIONMMX.o)
BUILTIN_MOTIONMMXEXT = $(PLUGIN_MOTIONMMXEXT:%.o=%-BUILTIN-MOTIONMMXEXT.o) \
		$(PLUGIN_MOTIONCOMMON:%.o=%-BUILTIN-MOTIONMMXEXT.o)

STD_PLUGIN_OBJ = $(PLUGIN_MOTION) $(PLUGIN_MOTIONMMX) $(PLUGIN_MOTIONMMXEXT) $(PLUGIN_MOTIONCOMMON)
ALL_OBJ = $(STD_PLUGIN_OBJ) $(BUILTIN_MOTION) $(BUILTIN_MOTIONMMX) $(BUILTIN_MOTIONMMXEXT)

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

$(BUILTIN_MOTION): %-BUILTIN-MOTION.o: .dep/%.d
$(BUILTIN_MOTION): %-BUILTIN-MOTION.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=motion -c -o $@ $<

$(BUILTIN_MOTIONMMX): %-BUILTIN-MOTIONMMX.o: .dep/%.d
$(BUILTIN_MOTIONMMX): %-BUILTIN-MOTIONMMX.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=motionmmx -c -o $@ $<

$(BUILTIN_MOTIONMMXEXT): %-BUILTIN-MOTIONMMXEXT.o: .dep/%.d
$(BUILTIN_MOTIONMMXEXT): %-BUILTIN-MOTIONMMXEXT.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=motionmmxext -c -o $@ $<

#
# Real targets
#
../../lib/motion.so: $(PLUGIN_MOTION) $(PLUGIN_MOTIONCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/motion.a: $(BUILTIN_MOTION)
	ar r $@ $^

../../lib/motionclassic.so: $(PLUGIN_MOTIONCLASSIC) $(PLUGIN_MOTIONCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/motionclassic.a: $(BUILTIN_MOTIONCLASSIC)
	ar r $@ $^

../../lib/motionmmx.so: $(PLUGIN_MOTIONMMX) $(PLUGIN_MOTIONCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/motionmmx.a: $(BUILTIN_MOTIONMMX)
	ar r $@ $^

../../lib/motionmmxext.so: $(PLUGIN_MOTIONMMXEXT) $(PLUGIN_MOTIONCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../../lib/motionmmxext.a: $(BUILTIN_MOTIONMMXEXT)
	ar r $@ $^

../../lib/motionaltivec.so: $(PLUGIN_MOTIONALTIVEC) $(PLUGIN_MOTIONCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) -framework vecLib

../../lib/motionaltivec.a: $(BUILTIN_MOTIONALTIVEC)
	ar r $@ $^

