# fluidlite

FLUID_GITURL := $(GITHUB)/divideconcept/FluidLite.git
FLUID_HASH := b0f187b404e393ee0a495b277154d55d7d03cbeb

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
	$(APPLY) $(SRC)/fluidlite/0001-pkg-config-add-FLUIDLITE_STATIC-define-when-using-th.patch
	$(call pkg_static,"fluidlite.pc.in")
	$(MOVE)

.fluidlite: fluidlite toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
