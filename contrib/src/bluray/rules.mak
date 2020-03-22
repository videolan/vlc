# LIBBLURAY

BLURAY_VERSION := 1.2.0
BLURAY_URL := $(VIDEOLAN)/libbluray/$(BLURAY_VERSION)/libbluray-$(BLURAY_VERSION).tar.bz2

ifdef BUILD_DISCS
ifndef HAVE_WINSTORE
PKGS += bluray
endif
endif
ifeq ($(call need_pkg,"libbluray >= 0.7.0"),)
PKGS_FOUND += bluray
endif

ifdef HAVE_ANDROID
WITH_FONTCONFIG = 0
else
ifdef HAVE_DARWIN_OS
WITH_FONTCONFIG = 0
else
ifdef HAVE_WIN32
WITH_FONTCONFIG = 0
else
WITH_FONTCONFIG = 1
endif
endif
endif

DEPS_bluray = libxml2 $(DEPS_libxml2) freetype2 $(DEPS_freetype2)

BLURAY_CONF = --disable-examples  \
              --with-libxml2

ifneq ($(WITH_FONTCONFIG), 0)
DEPS_bluray += fontconfig $(DEPS_fontconfig)
else
BLURAY_CONF += --without-fontconfig
endif

ifndef WITH_OPTIMIZATION
BLURAY_CONF += --disable-optimizations
endif

$(TARBALLS)/libbluray-$(BLURAY_VERSION).tar.bz2:
	$(call download,$(BLURAY_URL))

.sum-bluray: libbluray-$(BLURAY_VERSION).tar.bz2

bluray: libbluray-$(BLURAY_VERSION).tar.bz2 .sum-bluray
	$(UNPACK)
	$(APPLY) $(SRC)/bluray/0001-install-bdjo_data-header.patch
	$(call pkg_static,"src/libbluray.pc.in")
	$(MOVE)

.bluray: bluray
	rm -rf $(PREFIX)/share/java/libbluray*.jar
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure $(BLURAY_CONF) $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
