# freetype2

FREETYPE2_VERSION := 2.12.1
FREETYPE2_URL := $(SF)/freetype/freetype2/$(FREETYPE2_VERSION)/freetype-$(FREETYPE2_VERSION).tar.xz

PKGS += freetype2
ifeq ($(call need_pkg,"freetype2"),)
PKGS_FOUND += freetype2
endif

$(TARBALLS)/freetype-$(FREETYPE2_VERSION).tar.xz:
	$(call download_pkg,$(FREETYPE2_URL),freetype2)

.sum-freetype2: freetype-$(FREETYPE2_VERSION).tar.xz

freetype: freetype-$(FREETYPE2_VERSION).tar.xz .sum-freetype2
	$(UNPACK)
	$(call pkg_static, "builds/unix/freetype2.in")
	$(MOVE)

DEPS_freetype2 = zlib $(DEPS_zlib)

FREETYPE_CONF = -DFT_DISABLE_ZLIB=OFF -DFT_DISABLE_PNG=ON -DFT_DISABLE_BZIP2=NO \
                -DDISABLE_FORCE_DEBUG_POSTFIX:BOOL=ON -DFT_DISABLE_HARFBUZZ=ON \
                -DFT_DISABLE_BROTLI=ON

.freetype2: freetype toolchain.cmake
ifndef AD_CLAUSES
	$(REQUIRE_GPL)
endif
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(FREETYPE_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
