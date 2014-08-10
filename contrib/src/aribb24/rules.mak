# aribb24

ARIBB24_VERSION := 1.0.1
ARIBB24_URL := https://github.com/nkoriyama/aribb24/releases/download/v$(ARIBB24_VERSION)/aribb24-$(ARIBB24_VERSION).tar.bz2

PKGS += aribb24
ifeq ($(call need_pkg,"aribb24"),)
PKGS_FOUND += aribb24
endif

$(TARBALLS)/aribb24-$(ARIBB24_VERSION).tar.bz2:
	$(call download,$(ARIBB24_URL))

.sum-aribb24: aribb24-$(ARIBB24_VERSION).tar.bz2

aribb24: aribb24-$(ARIBB24_VERSION).tar.bz2 .sum-aribb24
	$(UNPACK)
	$(MOVE)

DEPS_aribb24 = png

.aribb24: aribb24
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
