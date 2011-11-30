# fluid

FLUID_VERSION := 1.1.3
FLUID_URL := $(SF)/fluidsynth/fluidsynth-$(FLUID_VERSION)/fluidsynth-$(FLUID_VERSION).tar.bz2
FLUID_OLDURL := $(SF)/fluidsynth/older%20releases/fluidsynth-1.0.9.tar.gz

PKGS += fluid
ifeq ($(call need_pkg,"fluidsynth"),)
PKGS_FOUND += fluid
endif

$(TARBALLS)/fluidsynth-$(FLUID_VERSION).tar.bz2:
	$(call download,$(FLUID_URL))

$(TARBALLS)/fluidsynth-1.0.9.tar.gz:
	$(call download,$(FLUID_OLDURL))

.sum-fluid: fluidsynth-$(FLUID_VERSION).tar.bz2 fluidsynth-1.0.9.tar.gz

ifeq ($(call need_pkg,"glib-2.0"),)
FLUID_TARBALL := fluidsynth-$(FLUID_VERSION).tar.bz2
else
FLUID_TARBALL := fluidsynth-1.0.9.tar.gz
FLUID_VERSION := 1.0.9
endif

fluidsynth: $(FLUID_TARBALL) .sum-fluid
	$(UNPACK)
	$(APPLY) $(SRC)/fluid/fluid-no-bin.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/fluid/fluid-static-win32.patch
endif
ifneq ($(FLUID_VERSION),1.0.9)
	$(APPLY) $(SRC)/fluid/fluid-pkg-static.patch
endif
	$(MOVE)

FLUIDCONF := $(HOSTCONF) \
	--disable-alsa-support \
	--disable-aufile-support \
	--disable-coreaudio \
	--disable-coremidi \
	--disable-dart \
	--disable-dbus-support \
	--disable-jack-support \
	--disable-ladcca \
	--disable-lash \
	--disable-libsndfile-support \
	--disable-midishare \
	--disable-oss-support \
  	--disable-portaudio-support \
	--disable-pulse-support \
	--without-readline

.fluid: fluidsynth
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DFLUIDSYNTH_NOT_A_DLL" ./configure $(FLUIDCONF)
	cd $< && $(MAKE) install
	touch $@
