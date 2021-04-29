# fluid

FLUID_VERSION := 2.1.8
FLUID_URL := $(GITHUB)/FluidSynth/fluidsynth/archive/refs/tags/v$(FLUID_VERSION).tar.gz

ifeq ($(call need_pkg,"glib-2.0 gthread-2.0"),)
PKGS += fluid
endif
ifeq ($(call need_pkg,"fluidsynth >= 1.1.2"),)
PKGS_FOUND += fluid
endif

DEPS_fluid = glib $(DEPS_glib)

$(TARBALLS)/fluidsynth-$(FLUID_VERSION).tar.gz:
	$(call download_pkg,$(FLUID_URL),fluid)

.sum-fluid: fluidsynth-$(FLUID_VERSION).tar.gz

fluidsynth: fluidsynth-$(FLUID_VERSION).tar.gz .sum-fluid
	$(UNPACK)
	$(APPLY) $(SRC)/fluid/fluid-pkg-static.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/fluid/fluid-static-win32.patch
endif
	$(MOVE)

FLUIDCONF := \
	-Denable-alsa=0 \
	-Denable-aufile=0 \
	-Denable-coreaudio=0 \
	-Denable-coremidi=0 \
	-Denable-dart=0 \
	-Denable-dbus=0 \
	-Denable-jack=0 \
	-Denable-lash=0 \
	-Denable-libsndfile=0 \
	-Denable-midishare=0 \
	-Denable-oss=0 \
	-Denable-portaudio=0 \
	-Denable-pulseaudio=0 \
	-Denable-readline=0

.fluid: fluidsynth toolchain.cmake
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DFLUIDSYNTH_NOT_A_DLL" $(CMAKE) $(FLUIDCONF)
	cd $< && $(CMAKEBUILD) . --target install
	touch $@
