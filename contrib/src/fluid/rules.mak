# fluid

 # DO NOT update, this will require glib
FLUID_VERSION := 1.0.9
#FLUID_URL := http://download.savannah.gnu.org/releases/fluid/fluidsynth-$(FLUID_VERSION).tar.gz
FLUID_URL := $(SF)/fluidsynth/older%20releases/fluidsynth-$(FLUID_VERSION).tar.gz

$(TARBALLS)/fluidsynth-$(FLUID_VERSION).tar.gz:
	$(call download,$(FLUID_URL))

.sum-fluid: fluidsynth-$(FLUID_VERSION).tar.gz

fluidsynth: fluidsynth-$(FLUID_VERSION).tar.gz .sum-fluid
	$(UNPACK)
	$(APPLY) $(SRC)/fluid/fluid-no-bin.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/fluid/fluid-static-win32.patch
endif
	$(MOVE)

FLUIDCONF := $(HOSTCONF) \
	--disable-alsa-support \
	--disable-coreaudio \
	--disable-coremidi \
	--disable-dart \
	--disable-jack-support \
	--disable-ladcca \
	--disable-lash \
	--disable-midishare \
	--disable-oss-support \
  	--disable-portaudio-support \
	--disable-pulse-support \
	--without-readline

.fluid: fluidsynth
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(FLUIDCONF)
	cd $< && $(MAKE) install
	touch $@
