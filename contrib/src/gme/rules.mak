# Game Music Emu

GME_VERSION := 0.6.0
GME_URL := http://game-music-emu.googlecode.com/files/game-music-emu-$(GME_VERSION).tar.bz2

PKGS += gme

$(TARBALLS)/game-music-emu-$(GME_VERSION).tar.bz2:
	$(call download,$(GME_URL))

.sum-gme: game-music-emu-$(GME_VERSION).tar.bz2

game-music-emu: game-music-emu-$(GME_VERSION).tar.bz2 .sum-gme
	$(UNPACK)
	$(APPLY) $(SRC)/gme/gme-static.patch
	$(MOVE)

.gme: game-music-emu toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) .
	cd $< && $(MAKE) install
	touch $@
