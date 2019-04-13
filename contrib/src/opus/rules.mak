# opus

OPUS_VERSION := 1.3.1

OPUS_URL := https://archive.mozilla.org/pub/opus/opus-$(OPUS_VERSION).tar.gz

PKGS += opus
ifeq ($(call need_pkg,"opus >= 0.9.14"),)
PKGS_FOUND += opus
endif

$(TARBALLS)/opus-$(OPUS_VERSION).tar.gz:
	$(call download_pkg,$(OPUS_URL),opus)

.sum-opus: opus-$(OPUS_VERSION).tar.gz

opus: opus-$(OPUS_VERSION).tar.gz .sum-opus
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

OPUS_CONF= --disable-extra-programs --disable-doc
ifndef HAVE_FPU
OPUS_CONF += --enable-fixed-point
endif

.opus: opus
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(OPUS_CONF)
	cd $< && $(MAKE) install
	touch $@
