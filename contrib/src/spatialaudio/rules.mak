# Spatialaudio

SPATIALAUDIO_VERSION := 0.3.0
SPATIALAUDIO_URL = https://github.com/videolabs/libspatialaudio/releases/download/$(SPATIALAUDIO_VERSION)/spatialaudio-$(SPATIALAUDIO_VERSION).tar.bz2

DEPS_spatialaudio = zlib mysofa

PKGS += spatialaudio

ifeq ($(call need_pkg,"spatialaudio"),)
PKGS_FOUND += spatialaudio
endif

$(TARBALLS)/spatialaudio-$(SPATIALAUDIO_VERSION).tar.bz2:
	$(call download_pkg,$(SPATIALAUDIO_URL),spatialaudio)

.sum-spatialaudio: spatialaudio-$(SPATIALAUDIO_VERSION).tar.bz2

spatialaudio: spatialaudio-$(SPATIALAUDIO_VERSION).tar.bz2 .sum-spatialaudio
	$(UNPACK)
	$(MOVE)

.spatialaudio: spatialaudio toolchain.cmake
	cd $< && rm -f CMakeCache.txt
	cd $< && $(HOSTVARS) $(CMAKE) -DMYSOFA_ROOT_DIR=$(PREFIX) -DHAVE_MIT_HRTF=OFF
	cd $< && $(MAKE) install
	touch $@
