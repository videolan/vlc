# fluidlite

FLUID_GITURL := https://github.com/divideconcept/FluidLite.git
FLUID_HASH := 9fca275

ifdef HAVE_WIN32
PKGS += fluidlite
endif

ifdef HAVE_MACOSX
PKGS += fluidlite
endif

ifeq ($(call need_pkg,"fluidlite"),)
PKGS_FOUND += fluidlite
endif

$(TARBALLS)/fluidlite-git.tar.xz:
	$(call download_git,$(FLUID_GITURL),,$(FLUID_HASH))

.sum-fluidlite: fluidlite-git.tar.xz
	$(warning $@ not implemented)
	touch $@

DEPS_fluidlite = ogg $(DEPS_ogg)

fluidlite: fluidlite-git.tar.xz .sum-fluidlite
	rm -Rf $@-git
	mkdir -p $@-git
	$(XZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
	$(MOVE)

.fluidlite: fluidlite toolchain.cmake
	-cd $< && rm CMakeCache.txt
	cd $< && $(HOSTVARS) $(CMAKE)
	cd $< && $(MAKE) install
	touch $@
