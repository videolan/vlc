# fluid

FLUID_VERSION := 1.1.6
FLUID_URL := $(SF)/fluidsynth/fluidsynth-$(FLUID_VERSION)/fluidsynth-$(FLUID_VERSION).tar.bz2

ifeq ($(call need_pkg,"glib-2.0 gthread-2.0"),)
PKGS += fluid
endif
ifeq ($(call need_pkg,"fluidsynth >= 1.1.2"),)
PKGS_FOUND += fluid
endif

DEPS_fluid = glib $(DEPS_glib)

$(TARBALLS)/fluidsynth-$(FLUID_VERSION).tar.bz2:
	$(call download_pkg,$(FLUID_URL),fluid)

.sum-fluid: fluidsynth-$(FLUID_VERSION).tar.bz2

fluidsynth: fluidsynth-$(FLUID_VERSION).tar.bz2 .sum-fluid
	$(UNPACK)
	$(APPLY) $(SRC)/fluid/fluid-no-bin.patch
	$(APPLY) $(SRC)/fluid/fluid-pkg-static.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/fluid/fluid-static-win32.patch
endif
	# Remove symbolic links to /usr/share/aclocal
	cd $(UNPACK_DIR)/m4/ && rm -f libtool.m4 lt*m4
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
