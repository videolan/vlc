# Spatialaudio

SPATIALAUDIO_VERSION := 0.2.0
SPATIALAUDIO_URL = https://github.com/videolabs/libspatialaudio/releases/download/$(SPATIALAUDIO_VERSION)/spatialaudio-$(SPATIALAUDIO_VERSION).tar.gz

DEPS_spatialaudio = zlib mysofa

PKGS += spatialaudio

ifeq ($(call need_pkg,"spatialaudio"),)
PKGS_FOUND += spatialaudio
endif

$(TARBALLS)/spatialaudio-$(SPATIALAUDIO_VERSION).tar.gz:
	$(call download_pkg,$(SPATIALAUDIO_URL),spatialaudio)

.sum-spatialaudio: spatialaudio-$(SPATIALAUDIO_VERSION).tar.gz

spatialaudio: spatialaudio-$(SPATIALAUDIO_VERSION).tar.gz .sum-spatialaudio
	$(UNPACK)
	$(MOVE)

.spatialaudio: spatialaudio toolchain.cmake
	-cd $< && rm CMakeCache.txt
	cd $< && $(HOSTVARS) $(CMAKE) -DMYSOFA_ROOT_DIR=$(PREFIX)
	cd $< && $(MAKE) install
	touch $@
