# LIBARCHIVE
LIBARCHIVE_VERSION := 3.6.1
LIBARCHIVE_URL := http://www.libarchive.org/downloads/libarchive-$(LIBARCHIVE_VERSION).tar.gz

PKGS += libarchive
ifeq ($(call need_pkg,"libarchive >= 3.2.0"),)
PKGS_FOUND += libarchive
endif

DEPS_libarchive = zlib $(DEPS_zlib)

LIBARCHIVE_CONF := \
		--disable-bsdcpio --disable-bsdtar --disable-bsdcat \
		--without-nettle --without-cng \
		--without-xml2 --without-lzma --without-iconv --without-expat

ifdef HAVE_WIN32
LIBARCHIVE_CONF += --without-openssl
endif

$(TARBALLS)/libarchive-$(LIBARCHIVE_VERSION).tar.gz:
	$(call download_pkg,$(LIBARCHIVE_URL),libarchive)

.sum-libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz

libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz .sum-libarchive
	$(UNPACK)
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/libarchive/android.patch
endif
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/libarchive/winrt.patch
endif
	$(call pkg_static,"build/pkgconfig/libarchive.pc.in")
	$(MOVE)

.libarchive: libarchive
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(LIBARCHIVE_CONF)
	+$(MAKEBUILD) install
	touch $@
