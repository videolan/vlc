# SAMPLERATE
SAMPLERATE_VERSION := 0.2.2
SAMPLERATE_URL := $(GITHUB)/libsndfile/libsamplerate/releases/download/$(SAMPLERATE_VERSION)/libsamplerate-$(SAMPLERATE_VERSION).tar.xz

ifdef GPL
PKGS += samplerate
endif
ifeq ($(call need_pkg,"samplerate"),)
PKGS_FOUND += samplerate
endif

$(TARBALLS)/libsamplerate-$(SAMPLERATE_VERSION).tar.xz:
	$(call download_pkg,$(SAMPLERATE_URL),samplerate)

.sum-samplerate: libsamplerate-$(SAMPLERATE_VERSION).tar.xz

samplerate: libsamplerate-$(SAMPLERATE_VERSION).tar.xz .sum-samplerate
	$(UNPACK)
	$(call update_autoconfig,build-aux)
	$(MOVE)

.samplerate: samplerate
	$(REQUIRE_GPL)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	+$(MAKEBUILD) install-data
	touch $@
