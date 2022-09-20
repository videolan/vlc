# fontconfig

FONTCONFIG_VERSION := 2.12.6
FONTCONFIG_URL := https://www.freedesktop.org/software/fontconfig/release/fontconfig-$(FONTCONFIG_VERSION).tar.gz

ifndef HAVE_WIN32
PKGS += fontconfig
endif
ifeq ($(call need_pkg,"fontconfig >= 2.11"),)
PKGS_FOUND += fontconfig
endif

$(TARBALLS)/fontconfig-$(FONTCONFIG_VERSION).tar.gz:
	$(call download_pkg,$(FONTCONFIG_URL),fontconfig)

.sum-fontconfig: fontconfig-$(FONTCONFIG_VERSION).tar.gz

fontconfig: fontconfig-$(FONTCONFIG_VERSION).tar.gz .sum-fontconfig
	$(UNPACK)
	$(RM) $(UNPACK_DIR)/src/fcobjshash.gperf
	# include the generated fcobjshash.h, not the one from src/
	sed -i.orig -e 's,"fcobjshash.h",<fcobjshash.h>,' $(UNPACK_DIR)/src/fcobjs.c
	$(call pkg_static, "fontconfig.pc.in")
	$(MOVE)

FONTCONFIG_CONF := --enable-libxml2 --disable-docs
FONTCONFIG_INSTALL :=
ifdef HAVE_MACOSX
# fc-cache crashes on macOS
FONTCONFIG_INSTALL += RUN_FC_CACHE_TEST=false
endif

# FreeType flags
ifneq ($(findstring freetype2,$(PKGS)),)
FONTCONFIG_CONF += --with-freetype-config="$(PREFIX)/bin/freetype-config"
endif

ifdef HAVE_CROSS_COMPILE
FONTCONFIG_CONF += --with-arch=$(ARCH)
endif

ifdef HAVE_MACOSX
FONTCONFIG_CONF += \
	--with-cache-dir=~/Library/Caches/fontconfig \
	--with-default-fonts=/System/Library/Fonts \
	--with-add-fonts=/Library/Fonts,~/Library/Fonts
endif

DEPS_fontconfig = freetype2 $(DEPS_freetype2) libxml2 $(DEPS_libxml2)

.fontconfig: fontconfig
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(FONTCONFIG_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) $(FONTCONFIG_INSTALL) install
	touch $@
