# freetype2

FREETYPE2_VERSION := 2.13.1
FREETYPE2_URL := $(SF)/freetype/freetype2/$(FREETYPE2_VERSION)/freetype-$(FREETYPE2_VERSION).tar.xz

PKGS += freetype2
ifeq ($(BUILD_WITH_FONTCONFIG), 1)
ifeq ($(call need_pkg,"freetype2 >= 21.0.15"),)
PKGS_FOUND += freetype2
endif
else # without fontconfig
ifeq ($(call need_pkg,"freetype2 >= 20.0.14"),)
PKGS_FOUND += freetype2
endif
endif

$(TARBALLS)/freetype-$(FREETYPE2_VERSION).tar.xz:
	$(call download_pkg,$(FREETYPE2_URL),freetype2)

.sum-freetype2: freetype-$(FREETYPE2_VERSION).tar.xz

freetype: freetype-$(FREETYPE2_VERSION).tar.xz .sum-freetype2
	$(UNPACK)
	$(MOVE)

DEPS_freetype2 = zlib $(DEPS_zlib)

FREETYPE_CONF := -Dpng=disabled -Dbzip2=disabled -Dharfbuzz=disabled \
                 -Dbrotli=disabled

.freetype2: freetype crossfile.meson
ifndef AD_CLAUSES
	$(REQUIRE_GPL)
endif
	$(MESONCLEAN)
	$(MESON) $(FREETYPE_CONF)
	+$(MESONBUILD)
	touch $@
