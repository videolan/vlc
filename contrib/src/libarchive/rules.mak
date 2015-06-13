# LIBARCHIVE
LIBARCHIVE_VERSION := 3.1.2
LIBARCHIVE_URL := http://www.libarchive.org/downloads/libarchive-$(LIBARCHIVE_VERSION).tar.gz

PKGS += libarchive
ifeq ($(call need_pkg,"libarchive >= 3.1.0"),)
PKGS_FOUND += libarchive
endif

$(TARBALLS)/libarchive-$(LIBARCHIVE_VERSION).tar.gz:
	$(call download,$(LIBARCHIVE_URL))

.sum-libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz

libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz .sum-libarchive
	$(UNPACK)
	$(APPLY) $(SRC)/libarchive/0001-Fix-build-failure-without-STATVFS.patch
	$(MOVE)

.libarchive: libarchive
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) \
		--disable-bsdcpio --disable-bsdtar --without-nettle --without-bz2lib \
		--without-xml2 --without-lzmadec --without-iconv --without-expat
	cd $< && $(MAKE) install
	touch $@
