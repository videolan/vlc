# Game Music Emu

GME_VERSION := 0.6.4
GME_URL := $(GITHUB)/libgme/game-music-emu/archive/refs/tags/$(GME_VERSION).tar.gz

PKGS += gme

$(TARBALLS)/game-music-emu-$(GME_VERSION).tar.gz:
	$(call download_pkg,$(GME_URL),gme)

DEPS_gme = zlib $(DEPS_zlib)

.sum-gme: game-music-emu-$(GME_VERSION).tar.gz

game-music-emu: game-music-emu-$(GME_VERSION).tar.gz .sum-gme
	$(UNPACK)
	$(APPLY) $(SRC)/gme/0001-Export-the-proper-C-runtime-library.patch
	$(call pkg_static,"gme/libgme.pc.in")
	$(MOVE)

GME_CONF := -DENABLE_UBSAN=OFF

.gme: game-music-emu toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(GME_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
