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
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/fontconfig/fontconfig-win32.patch
	$(APPLY) $(SRC)/fontconfig/fontconfig-noxml2.patch
endif
	$(RM) $(UNPACK_DIR)/src/fcobjshash.gperf
	$(call pkg_static, "fontconfig.pc.in")
	$(MOVE)

FONTCONFIG_CONF := --enable-libxml2 --disable-docs

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
ifdef HAVE_WIN32
	$(RECONF)
endif
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(FONTCONFIG_CONF)
	$(MAKE) -C $<
ifndef HAVE_MACOSX
	$(MAKE) -C $< install
else
	$(MAKE) -C $< install-exec
	$(MAKE) -C $< -C fontconfig
	$(MAKE) -C $< -C fontconfig install-data
	sed -e 's%/usr/lib/libiconv.la%%' -i.orig $(PREFIX)/lib/libfontconfig.la
	cp $</fontconfig.pc $(PREFIX)/lib/pkgconfig/
endif
	touch $@
