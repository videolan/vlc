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
	$(APPLY) $(SRC)/gme/0001-Export-the-proper-C-runtime-library.patch
	$(APPLY) $(SRC)/gme/0002-link-with-libm-and-set-it-in-pkg-config-when-buildin.patch
	$(APPLY) $(SRC)/gme/0003-fix-android-toolchain-broken-CMAKE_CXX_IMPLICIT_LINK.patch
	$(call pkg_static,"gme/libgme.pc.in")
	$(MOVE)

GME_CONF := -DENABLE_UBSAN=OFF

.gme: game-music-emu toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) . $(GME_CONF)
	+$(CMAKEBUILD) $< --target install
	touch $@
