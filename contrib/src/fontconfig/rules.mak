# fontconfig

FONTCONFIG_VERSION := 2.18.1
FONTCONFIG_URL := https://gitlab.freedesktop.org/api/v4/projects/890/packages/generic/fontconfig/$(FONTCONFIG_VERSION)/fontconfig-$(FONTCONFIG_VERSION).tar.xz

ifneq ($(BUILD_WITH_FONTCONFIG), 0)
PKGS += fontconfig
endif
ifeq ($(call need_pkg,"fontconfig >= 2.11"),)
PKGS_FOUND += fontconfig
endif

$(TARBALLS)/fontconfig-$(FONTCONFIG_VERSION).tar.xz:
	$(call download_pkg,$(FONTCONFIG_URL),fontconfig)

.sum-fontconfig: fontconfig-$(FONTCONFIG_VERSION).tar.xz

fontconfig: fontconfig-$(FONTCONFIG_VERSION).tar.xz .sum-fontconfig
	$(UNPACK)
	$(call update_autoconfig,.)
	$(call pkg_static, "fontconfig.pc.in")
	$(MOVE)

FONTCONFIG_CONF := --enable-libxml2 --disable-docs --disable-cache-build

ifdef HAVE_CROSS_COMPILE
FONTCONFIG_CONF += --with-arch=$(ARCH)
endif

ifdef HAVE_MACOSX
FONTCONFIG_CONF += \
	--with-cache-dir=~/Library/Caches/fontconfig
endif

ifdef HAVE_ANDROID
FONTCONFIG_CONF += \
	--with-cache-dir=~/.cache/fontconfig \
	--with-default-fonts=/system/fonts \
	--with-add-fonts=/product/fonts,/data/fonts
endif

DEPS_fontconfig = freetype2 $(DEPS_freetype2) libxml2 $(DEPS_libxml2)

# assume va_copy works as the test fails when cross compiling
FONTCONFIG_CONF += ac_cv_va_copy=.C99

.fontconfig: fontconfig
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(FONTCONFIG_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
