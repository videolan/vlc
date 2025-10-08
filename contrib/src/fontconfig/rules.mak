# fontconfig

FONTCONFIG_VERSION := 2.14.2
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
	$(APPLY) $(SRC)/fontconfig/0001-detect-mkostemp-with-stdlib.h.patch
	$(MOVE)

FONTCONFIG_CONF := -Ddoc=disabled -Dtests=disabled -Dtools=disabled -Dnls=disabled

ifdef HAVE_MACOSX
FONTCONFIG_CONF += \
	-Dcache-dir=~/Library/Caches/fontconfig \
	-Ddefault-fonts-dirs=/System/Library/Fonts \
	-Dadditional-fonts-dirs=/Library/Fonts,~/Library/Fonts
endif

DEPS_fontconfig = freetype2 $(DEPS_freetype2) libxml2 $(DEPS_libxml2)

.fontconfig: fontconfig crossfile.meson
	$(MESONCLEAN)
	$(MESON) $(FONTCONFIG_CONF)
	+$(MESONBUILD)
	touch $@
