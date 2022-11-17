# fluid

FLUID_VERSION := 2.3.0
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
	$(call pkg_static,"fluidsynth.pc.in")
	# don't use their internal windows-version variable to set the Windows version
	sed -i.orig 's,.*$${windows-version}.*,# use our Windows version,' "$(UNPACK_DIR)/CMakeLists.txt"
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

ifdef HAVE_LINUX
# don't use openmp as the linking fails
# https://github.com/FluidSynth/fluidsynth/issues/904 is not properly fixed
FLUIDCONF += -Denable-openmp=0
endif

.fluid: fluidsynth toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE_PIC) $(FLUIDCONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
