# Game Music Emu

GME_VERSION := 0.6.3
GME_URL := https://bitbucket.org/mpyne/game-music-emu/downloads/game-music-emu-$(GME_VERSION).tar.xz

PKGS += gme

$(TARBALLS)/game-music-emu-$(GME_VERSION).tar.xz:
	$(call download_pkg,$(GME_URL),gme)

DEPS_gme = zlib $(DEPS_zlib)

.sum-gme: game-music-emu-$(GME_VERSION).tar.xz

game-music-emu: game-music-emu-$(GME_VERSION).tar.xz .sum-gme
	$(UNPACK)
	$(APPLY) $(SRC)/gme/skip-underrun.patch
	$(APPLY) $(SRC)/gme/add-libm.patch
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/gme/mac-use-c-stdlib.patch
endif
	$(call pkg_static,"gme/libgme.pc.in")
	$(MOVE)

.gme: game-music-emu toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) . -DENABLE_UBSAN=OFF
	+$(CMAKEBUILD) $< --target install
	touch $@
