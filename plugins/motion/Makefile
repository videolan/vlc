###############################################################################
# vlc (VideoLAN Client) motion module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_MOTION = motion.o vdec_motion_inner.o
PLUGIN_MOTIONMMX = motionmmx.o vdec_motion_inner_mmx.o
PLUGIN_MOTIONMMXEXT = motionmmxext.o vdec_motion_inner_mmxext.o
PLUGIN_MOTIONCOMMON = vdec_motion_common.o

BUILTIN_MOTION = $(PLUGIN_MOTION:%.o=BUILTIN_MOTION_%.o) \
		$(PLUGIN_MOTIONCOMMON:%.o=BUILTIN_MOTION_%.o)
BUILTIN_MOTIONMMX = $(PLUGIN_MOTIONMMX:%.o=BUILTIN_MOTIONMMX_%.o) \
		$(PLUGIN_MOTIONCOMMON:%.o=BUILTIN_MOTIONMMX_%.o)
BUILTIN_MOTIONMMXEXT = $(PLUGIN_MOTIONMMXEXT:%.o=BUILTIN_MOTIONMMXEXT_%.o) \
		$(PLUGIN_MOTIONCOMMON:%.o=BUILTIN_MOTIONMMXEXT_%.o)

PLUGIN_C = $(PLUGIN_MOTION) $(PLUGIN_MOTIONMMX) $(PLUGIN_MOTIONMMXEXT) $(PLUGIN_MOTIONCOMMON)
ALL_OBJ = $(PLUGIN_C) $(BUILTIN_MOTION) $(BUILTIN_MOTIONMMX) $(BUILTIN_MOTIONMMXEXT)

#
# Virtual targets
#

include ../../Makefile.modules

$(BUILTIN_MOTION): BUILTIN_MOTION_%.o: .dep/%.d
$(BUILTIN_MOTION): BUILTIN_MOTION_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=motion -c -o $@ $<

$(BUILTIN_MOTIONMMX): BUILTIN_MOTIONMMX_%.o: .dep/%.d
$(BUILTIN_MOTIONMMX): BUILTIN_MOTIONMMX_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=motionmmx -c -o $@ $<

$(BUILTIN_MOTIONMMXEXT): BUILTIN_MOTIONMMXEXT_%.o: .dep/%.d
$(BUILTIN_MOTIONMMXEXT): BUILTIN_MOTIONMMXEXT_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=motionmmxext -c -o $@ $<

#
# Real targets
#

../motion.so: $(PLUGIN_MOTION) $(PLUGIN_MOTIONCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../motion.a: $(BUILTIN_MOTION)
	ar r $@ $^
	$(RANLIB) $@

../motionclassic.so: $(PLUGIN_MOTIONCLASSIC) $(PLUGIN_MOTIONCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../motionclassic.a: $(BUILTIN_MOTIONCLASSIC)
	ar r $@ $^
	$(RANLIB) $@

../motionmmx.so: $(PLUGIN_MOTIONMMX) $(PLUGIN_MOTIONCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../motionmmx.a: $(BUILTIN_MOTIONMMX)
	ar r $@ $^
	$(RANLIB) $@

../motionmmxext.so: $(PLUGIN_MOTIONMMXEXT) $(PLUGIN_MOTIONCOMMON)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../motionmmxext.a: $(BUILTIN_MOTIONMMXEXT)
	ar r $@ $^
	$(RANLIB) $@

