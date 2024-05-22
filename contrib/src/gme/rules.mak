# Game Music Emu

GME_VERSION := 0.6.3
GME_URL := $(GITHUB)/libgme/game-music-emu/archive/refs/tags/$(GME_VERSION).tar.gz

PKGS += gme

$(TARBALLS)/game-music-emu-$(GME_VERSION).tar.gz:
	$(call download_pkg,$(GME_URL),gme)

DEPS_gme = zlib $(DEPS_zlib)

.sum-gme: game-music-emu-$(GME_VERSION).tar.gz

game-music-emu: game-music-emu-$(GME_VERSION).tar.gz .sum-gme
	$(UNPACK)
	$(APPLY) $(SRC)/gme/0001-don-t-skip-negative-fixes-14088.patch
	$(APPLY) $(SRC)/gme/0001-Export-the-proper-C-runtime-library.patch
	$(APPLY) $(SRC)/gme/0002-link-with-libm-and-set-it-in-pkg-config-when-buildin.patch
	$(APPLY) $(SRC)/gme/0003-fix-android-toolchain-broken-CMAKE_CXX_IMPLICIT_LINK.patch
	$(APPLY) $(SRC)/gme/0004-Blip_Buffer-replace-assert-with-a-check.patch
	$(call pkg_static,"gme/libgme.pc.in")
	$(MOVE)

GME_CONF := -DENABLE_UBSAN=OFF

.gme: game-music-emu toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(GME_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
