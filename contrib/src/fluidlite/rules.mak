# fluidlite

FLUID_GITURL := https://github.com/divideconcept/FluidLite.git
FLUID_HASH := a95c0303a40deb335dd3e51a8a783bb99a403c31

ifdef HAVE_WIN32
PKGS += fluidlite
endif

ifeq ($(call need_pkg,"fluidlite"),)
PKGS_FOUND += fluidlite
endif

$(TARBALLS)/fluidlite-$(FLUID_HASH).tar.xz:
	$(call download_git,$(FLUID_GITURL),,$(FLUID_HASH))

.sum-fluidlite: fluidlite-$(FLUID_HASH).tar.xz
	$(call check_githash,$(FLUID_HASH))
	touch $@

DEPS_fluidlite = ogg $(DEPS_ogg)

fluidlite: fluidlite-$(FLUID_HASH).tar.xz .sum-fluidlite
	$(UNPACK)
	$(APPLY) $(SRC)/fluidlite/add-pic.diff
	$(MOVE)

.fluidlite: fluidlite toolchain.cmake
	cd $< && rm -f CMakeCache.txt
	cd $< && $(HOSTVARS) $(CMAKE)
	cd $< && $(MAKE) install
	touch $@
