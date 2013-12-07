# opus

OPUS_VERSION := 1.1

OPUS_URL := http://downloads.xiph.org/releases/opus/opus-$(OPUS_VERSION).tar.gz

PKGS += opus
ifeq ($(call need_pkg,"opus >= 0.9.14"),)
PKGS_FOUND += opus
endif

$(TARBALLS)/opus-$(OPUS_VERSION).tar.gz:
	$(call download,$(OPUS_URL))

.sum-opus: opus-$(OPUS_VERSION).tar.gz

opus: opus-$(OPUS_VERSION).tar.gz .sum-opus
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

OPUS_CONF=
ifndef HAVE_FPU
OPUS_CONF += --enable-fixed-point
endif

.opus: opus
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(OPUS_CONF)
	cd $< && $(MAKE) install
	touch $@
