# aribb24

ARIBB24_VERSION := 1.0.3
ARIBB24_URL := https://github.com/nkoriyama/aribb24/archive/v$(ARIBB24_VERSION).tar.gz

ifdef GPL
ifdef GNUV3
PKGS += aribb24
endif
endif
ifeq ($(call need_pkg,"aribb24"),)
PKGS_FOUND += aribb24
endif

$(TARBALLS)/aribb24-$(ARIBB24_VERSION).tar.gz:
	$(call download_pkg,$(ARIBB24_URL),aribb24)

.sum-aribb24: aribb24-$(ARIBB24_VERSION).tar.gz

aribb24: aribb24-$(ARIBB24_VERSION).tar.gz .sum-aribb24
	$(UNPACK)
	$(APPLY) $(SRC)/aribb24/libm.patch
	$(call pkg_static,"src/aribb24.pc.in")
	$(MOVE)

DEPS_aribb24 = png

.aribb24: aribb24
	$(REQUIRE_GPL)
	$(REQUIRE_GNUV3)
	cd $< && $(SHELL) ./bootstrap
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
