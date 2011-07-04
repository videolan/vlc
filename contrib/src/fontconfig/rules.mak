# fontconfig

FONTCONFIG_VERSION := 2.8.0
FONTCONFIG_URL := http://fontconfig.org/release/fontconfig-$(FONTCONFIG_VERSION).tar.gz

PKGS += fontconfig

$(TARBALLS)/fontconfig-$(FONTCONFIG_VERSION).tar.gz:
	$(call download,$(FONTCONFIG_URL))

.sum-fontconfig: fontconfig-$(FONTCONFIG_VERSION).tar.gz

fontconfig: fontconfig-$(FONTCONFIG_VERSION).tar.gz .sum-fontconfig
	$(UNPACK)
	$(APPLY) $(SRC)/fontconfig/fontconfig-march.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/fontconfig/fontconfig-win32.patch
	$(APPLY) $(SRC)/fontconfig/fontconfig-noxml2.patch
endif
	$(MOVE)

FONTCONFIG_BASE_CONF = --prefix=$(PREFIX) \
					   --with-freetype-config=$(PREFIX)/bin/freetype-config \
					   --enable-libxml2 \
					   --disable-docs

FONTCONFIG_CONF-$(ENABLED)      = $(HOSTCONF) $(FONTCONFIG_BASE_CONF)
FONTCONFIG_CONF-$(HAVE_WIN32)   = $(HOSTCONF) --with-freetype-config=$(PREFIX)/bin/freetype-config --disable-docs --with-arch=i686
FONTCONFIG_CONF-$(HAVE_MACOSX) += $(HOSTCONF) \
	--with-cache-dir=~/Library/Caches/fontconfig \
	--with-confdir=/usr/X11/lib/X11/fonts \
	--with-default-fonts=/System/Library/Fonts \
	--with-add-fonts=/Library/Fonts,~/Library/Fonts  \
	--with-arch=$(ARCH)

FONTCONFIG_ENV-$(ENABLED)         = $(HOSTCC) LIBXML2_CFLAGS=`$(PREFIX)/bin/xml2-config --cflags`
FONTCONFIG_ENV-$(HAVE_MACOSX)     = $(HOSTCC) LIBXML2_CFLAGS=`xml2-config --cflags` LIBXML2_LIBS=`xml2-config --libs`
FONTCONFIG_ENV-$(HAVE_WIN32)      = $(HOSTCC)

DEPS_fontconfig = freetype2 $(DEPS_freetype2) libxml2 $(DEPS_libxml2)

.fontconfig: fontconfig
ifdef HAVE_WIN32
	$(RECONF)
endif
	cd $<; $(FONTCONFIG_ENV-1) ./configure $(FONTCONFIG_CONF-1) && make
ifndef HAVE_MACOSX
	cd $<; make install
else
	cd $<; make install-exec && (cd fontconfig ; make install-data) && cp fontconfig.pc     $(PKG_CONFIG_LIBDIR) && sed -e 's%/usr/lib/libiconv.la%%' -i.orig $(PREFIX)/lib/libfontconfig.la
endif
	$(INSTALL_NAME)
	touch $@
