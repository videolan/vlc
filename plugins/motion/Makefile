###############################################################################
# vlc (VideoLAN Client) motion module makefile
# (c)2001 VideoLAN
###############################################################################

#
# Objects
#

PLUGIN_MOTION = motion.o
PLUGIN_MOTIONMMX = motionmmx.o
PLUGIN_MOTIONMMXEXT = motionmmxext.o
PLUGIN_MOTION3DNOW = motion3dnow.o
PLUGIN_MOTIONALTIVEC = motionaltivec.o

BUILTIN_MOTION = $(PLUGIN_MOTION:%.o=BUILTIN_MOTION_%.o)
BUILTIN_MOTIONMMX = $(PLUGIN_MOTIONMMX:%.o=BUILTIN_MOTIONMMX_%.o)
BUILTIN_MOTIONMMXEXT = $(PLUGIN_MOTIONMMXEXT:%.o=BUILTIN_MOTIONMMXEXT_%.o)
BUILTIN_MOTION3DNOW = $(PLUGIN_MOTION3DNOW:%.o=BUILTIN_MOTION3DNOW_%.o)
BUILTIN_MOTIONALTIVEC = $(PLUGIN_MOTIONALTIVEC:%.o=BUILTIN_MOTIONALTIVEC_%.o)

PLUGIN_C = $(PLUGIN_MOTION) $(PLUGIN_MOTIONMMX) $(PLUGIN_MOTIONMMXEXT) $(PLUGIN_MOTION3DNOW)
ALL_OBJ = $(PLUGIN_C) $(PLUGIN_MOTIONALTIVEC) $(BUILTIN_MOTION) $(BUILTIN_MOTIONMMX) $(BUILTIN_MOTIONMMXEXT) $(BUILTIN_MOTION3DNOW)

#
# Virtual targets
#

include ../../Makefile.modules

$(PLUGIN_MOTIONALTIVEC): %.o: .dep/%.d
$(PLUGIN_MOTIONALTIVEC): %.o: %.c
	$(CC) $(CFLAGS) -DPLUGIN $(PCFLAGS) -faltivec -c -o $@ $<

$(BUILTIN_MOTION): BUILTIN_MOTION_%.o: .dep/%.d
$(BUILTIN_MOTION): BUILTIN_MOTION_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=motion -c -o $@ $<

$(BUILTIN_MOTIONMMX): BUILTIN_MOTIONMMX_%.o: .dep/%.d
$(BUILTIN_MOTIONMMX): BUILTIN_MOTIONMMX_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=motionmmx -c -o $@ $<

$(BUILTIN_MOTIONMMXEXT): BUILTIN_MOTIONMMXEXT_%.o: .dep/%.d
$(BUILTIN_MOTIONMMXEXT): BUILTIN_MOTIONMMXEXT_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=motionmmxext -c -o $@ $<

$(BUILTIN_MOTION3DNOW): BUILTIN_MOTION3DNOW_%.o: .dep/%.d
$(BUILTIN_MOTION3DNOW): BUILTIN_MOTION3DNOW_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=motion3dnow -c -o $@ $<

$(BUILTIN_MOTIONALTIVEC): BUILTIN_MOTIONALTIVEC_%.o: .dep/%.d
$(BUILTIN_MOTIONALTIVEC): BUILTIN_MOTIONALTIVEC_%.o: %.c
	$(CC) $(CFLAGS) -DBUILTIN -DMODULE_NAME=motionaltivec -faltivec -c -o $@ $<

#
# Real targets
#

../motion.so: $(PLUGIN_MOTION)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../motion.a: $(BUILTIN_MOTION)
	ar r $@ $^
	$(RANLIB) $@

../motionclassic.so: $(PLUGIN_MOTIONCLASSIC)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../motionclassic.a: $(BUILTIN_MOTIONCLASSIC)
	ar r $@ $^
	$(RANLIB) $@

../motionmmx.so: $(PLUGIN_MOTIONMMX)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../motionmmx.a: $(BUILTIN_MOTIONMMX)
	ar r $@ $^
	$(RANLIB) $@

../motionmmxext.so: $(PLUGIN_MOTIONMMXEXT)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../motionmmxext.a: $(BUILTIN_MOTIONMMXEXT)
	ar r $@ $^
	$(RANLIB) $@

../motion3dnow.so: $(PLUGIN_MOTION3DNOW)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) 

../motion3dnow.a: $(BUILTIN_MOTION3DNOW)
	ar r $@ $^
	$(RANLIB) $@

../motionaltivec.so: $(PLUGIN_MOTIONALTIVEC)
	$(CC) $(PCFLAGS) -o $@ $^ $(PLCFLAGS) $(LIB_ALTIVEC)

../motionaltivec.a: $(BUILTIN_MOTIONALTIVEC)
	ar r $@ $^
	$(RANLIB) $@
