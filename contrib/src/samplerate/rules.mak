# SAMPLERATE
SAMPLERATE_VERSION := 0.1.9
SAMPLERATE_URL := $(GITHUB)/libsndfile/libsamplerate/releases/download/$(SAMPLERATE_VERSION)/libsamplerate-$(SAMPLERATE_VERSION).tar.gz

ifdef GPL
PKGS += samplerate
endif
ifeq ($(call need_pkg,"samplerate"),)
PKGS_FOUND += samplerate
endif

$(TARBALLS)/libsamplerate-$(SAMPLERATE_VERSION).tar.gz:
	$(call download_pkg,$(SAMPLERATE_URL),samplerate)

.sum-samplerate: libsamplerate-$(SAMPLERATE_VERSION).tar.gz

samplerate: libsamplerate-$(SAMPLERATE_VERSION).tar.gz .sum-samplerate
	$(UNPACK)
	$(call update_autoconfig,Cfg)
	$(MOVE)

.samplerate: samplerate
	$(REQUIRE_GPL)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE)
	+$(MAKEBUILD) -C src
	+$(MAKEBUILD) -C src install
	+$(MAKEBUILD) install-data
	touch $@
