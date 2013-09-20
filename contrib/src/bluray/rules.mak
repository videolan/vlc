# LIBBLURAY

ifdef BUILD_DISCS
PKGS += bluray
endif
ifeq ($(call need_pkg,"libbluray >= 0.2.1"),)
PKGS_FOUND += bluray
endif

DEPS_bluray = libxml2 $(DEPS_libxml2)

BLURAY_VERSION := 0.4.0
BLURAY_URL := http://ftp.videolan.org/pub/videolan/libbluray/$(BLURAY_VERSION)/libbluray-$(BLURAY_VERSION).tar.bz2

$(TARBALLS)/libbluray-$(BLURAY_VERSION).tar.bz2:
	$(call download,$(BLURAY_URL))

.sum-bluray: libbluray-$(BLURAY_VERSION).tar.bz2

bluray: libbluray-$(BLURAY_VERSION).tar.bz2 .sum-bluray
	$(UNPACK)
	$(MOVE)

.bluray: bluray
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure --disable-examples --disable-debug --enable-libxml2 $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
