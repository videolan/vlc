# fontconfig

FONTCONFIG_VERSION := 2.10.2
FONTCONFIG_URL := http://fontconfig.org/release/fontconfig-$(FONTCONFIG_VERSION).tar.gz

PKGS += fontconfig
ifeq ($(call need_pkg,"fontconfig"),)
PKGS_FOUND += fontconfig
endif

$(TARBALLS)/fontconfig-$(FONTCONFIG_VERSION).tar.gz:
	$(call download,$(FONTCONFIG_URL))

.sum-fontconfig: fontconfig-$(FONTCONFIG_VERSION).tar.gz

fontconfig: fontconfig-$(FONTCONFIG_VERSION).tar.gz .sum-fontconfig
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/fontconfig/fontconfig-win32.patch
	$(APPLY) $(SRC)/fontconfig/fontconfig-noxml2.patch
endif
	$(MOVE)

FONTCONFIG_CONF := $(HOSTCONF) \
	--enable-libxml2 \
	--disable-docs
FONTCONFIG_ENV := $(HOSTVARS)

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
	--with-confdir=/usr/X11/lib/X11/fonts \
	--with-default-fonts=/System/Library/Fonts \
	--with-add-fonts=/Library/Fonts,~/Library/Fonts
# libxml2 without pkg-config...
FONTCONFIG_ENV += LIBXML2_CFLAGS=`xml2-config --cflags`
FONTCONFIG_ENV += LIBXML2_LIBS=`xml2-config --libs`
endif

DEPS_fontconfig = freetype2 $(DEPS_freetype2) libxml2 $(DEPS_libxml2)

.fontconfig: fontconfig
ifdef HAVE_WIN32
	$(RECONF)
endif
	cd $< && $(FONTCONFIG_ENV) ./configure $(FONTCONFIG_CONF)
	cd $< && $(MAKE)
ifndef HAVE_MACOSX
	cd $< && $(MAKE) install
else
	cd $< && $(MAKE) install-exec
	cd $</fontconfig && $(MAKE) install-data
	sed -e 's%/usr/lib/libiconv.la%%' -i.orig $(PREFIX)/lib/libfontconfig.la
	cp $</fontconfig.pc $(PREFIX)/lib/pkgconfig/
endif
	touch $@
