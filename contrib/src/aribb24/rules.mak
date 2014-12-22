# aribb24

ARIBB24_VERSION := 1.0.3
ARIBB24_URL := https://github.com/nkoriyama/aribb24/archive/v$(ARIBB24_VERSION).tar.gz

PKGS += aribb24
ifeq ($(call need_pkg,"aribb24"),)
PKGS_FOUND += aribb24
endif

$(TARBALLS)/aribb24-$(ARIBB24_VERSION).tar.gz:
	$(call download,$(ARIBB24_URL))

.sum-aribb24: aribb24-$(ARIBB24_VERSION).tar.gz

aribb24: aribb24-$(ARIBB24_VERSION).tar.gz .sum-aribb24
	$(UNPACK)
	$(MOVE)

DEPS_aribb24 = png

.aribb24: aribb24
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
