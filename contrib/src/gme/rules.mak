# Game Music Emu

GME_VERSION := 0.6.5
GME_URL := $(GITHUB)/libgme/game-music-emu/archive/refs/tags/$(GME_VERSION).tar.gz

PKGS += gme

$(TARBALLS)/game-music-emu-$(GME_VERSION).tar.gz:
	$(call download_pkg,$(GME_URL),gme)

DEPS_gme = zlib $(DEPS_zlib)

.sum-gme: game-music-emu-$(GME_VERSION).tar.gz

game-music-emu: game-music-emu-$(GME_VERSION).tar.gz .sum-gme
	$(UNPACK)
	$(APPLY) $(SRC)/gme/ae10a8f05479e3c8f80429e7a59759fed06f8c13.patch
	$(call pkg_static,"gme/libgme.pc.in")
	$(MOVE)

GME_CONF := -DENABLE_UBSAN=OFF

.gme: game-music-emu toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(GME_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
