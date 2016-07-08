# fluidlite

FLUID_GITURL := https://github.com/divideconcept/FluidLite.git
FLUID_HASH := 171d222

$(TARBALLS)/fluidlite-git.tar.xz:
	$(call download_git,$(FLUID_GITURL),,$(FLUID_HASH))

.sum-fluidlite: fluidlite-git.tar.xz
	$(warning $@ not implemented)
	touch $@

fluidlite: fluidlite-git.tar.xz .sum-fluidlite
	rm -Rf $@-git
	mkdir -p $@-git
	$(XZCAT) "$<" | (cd $@-git && tar xv --strip-components=1)
	$(MOVE)

.fluidlite: fluidlite
	-cd $< && rm CMakeCache.txt
	cd $< && $(HOSTVARS) $(CMAKE)
	cd $< && $(MAKE) install
	cp $(SRC)/fluidlite/fluidlite.pc "$(PREFIX)/lib/pkgconfig"
	touch $@
