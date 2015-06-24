# LIBBLURAY

BLURAY_VERSION := 0.8.1
BLURAY_URL := $(VIDEOLAN)/libbluray/$(BLURAY_VERSION)/libbluray-$(BLURAY_VERSION).tar.bz2

ifdef BUILD_DISCS
PKGS += bluray
endif
ifeq ($(call need_pkg,"libbluray >= 0.5.0"),)
PKGS_FOUND += bluray
endif

ifdef HAVE_ANDROID
WITH_FONTCONFIG = 0
else
ifdef HAVE_IOS
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
              --with-libxml2      \
              --enable-bdjava

ifneq ($(WITH_FONTCONFIG), 0)
DEPS_bluray += fontconfig $(DEPS_fontconfig)
else
BLURAY_CONF += --without-fontconfig
endif

$(TARBALLS)/libbluray-$(BLURAY_VERSION).tar.bz2:
	$(call download,$(BLURAY_URL))

.sum-bluray: libbluray-$(BLURAY_VERSION).tar.bz2

bluray: libbluray-$(BLURAY_VERSION).tar.bz2 .sum-bluray
	$(UNPACK)
	$(MOVE)

.bluray: bluray
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure $(BLURAY_CONF) $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
