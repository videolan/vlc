# LIBBLURAY

BLURAY_VERSION := 1.4.0
BLURAY_URL := $(VIDEOLAN)/libbluray/$(BLURAY_VERSION)/libbluray-$(BLURAY_VERSION).tar.xz

ifdef BUILD_DISCS
ifndef HAVE_WINSTORE
PKGS += bluray
endif
endif
ifeq ($(call need_pkg,"libbluray >= 1.1.0"),)
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

BLURAY_CONF = -Dfreetype=enabled -Dlibxml2=enabled
ifdef HAVE_CROSS_COMPILE
BLURAY_CONF += -Denable_tools=false
endif

ifneq ($(WITH_FONTCONFIG), 0)
DEPS_bluray += fontconfig $(DEPS_fontconfig)
BLURAY_CONF += -Dfontconfig=enabled

else
BLURAY_CONF += -Dfontconfig=disabled
endif

$(TARBALLS)/libbluray-$(BLURAY_VERSION).tar.xz:
	$(call download,$(BLURAY_URL))

.sum-bluray: libbluray-$(BLURAY_VERSION).tar.xz

bluray: libbluray-$(BLURAY_VERSION).tar.xz .sum-bluray
	$(UNPACK)
	$(APPLY) $(SRC)/bluray/0001-Link-with-gdi32-when-using-freetype-in-Windows.patch
	$(MOVE)

.bluray: bluray crossfile.meson
	rm -rf $(PREFIX)/share/java/libbluray*.jar
	$(MESONCLEAN)
	$(MESON) $(BLURAY_CONF)
	+$(MESONBUILD)
	touch $@
