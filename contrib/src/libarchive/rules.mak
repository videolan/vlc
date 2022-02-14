# LIBARCHIVE
LIBARCHIVE_VERSION := 3.6.0
LIBARCHIVE_URL := http://www.libarchive.org/downloads/libarchive-$(LIBARCHIVE_VERSION).tar.gz

PKGS += libarchive
ifeq ($(call need_pkg,"libarchive >= 3.2.0"),)
PKGS_FOUND += libarchive
endif

DEPS_libarchive = zlib

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
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) \
		--disable-bsdcpio --disable-bsdtar --disable-bsdcat \
		--without-nettle --without-cng \
		--without-xml2 --without-lzma --without-iconv --without-expat
	cd $< && $(MAKE) install
	touch $@
