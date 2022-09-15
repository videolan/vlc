# libxml2

LIBXML2_VERSION := 2.9.14
LIBXML2_URL := https://download.gnome.org/sources/libxml2/2.9/libxml2-$(LIBXML2_VERSION).tar.xz

PKGS += libxml2
ifeq ($(call need_pkg,"libxml-2.0"),)
PKGS_FOUND += libxml2
endif

$(TARBALLS)/libxml2-$(LIBXML2_VERSION).tar.xz:
	$(call download_pkg,$(LIBXML2_URL),libxml2)

.sum-libxml2: libxml2-$(LIBXML2_VERSION).tar.xz

XMLCONF = --with-minimal     \
          --with-catalog     \
          --with-reader      \
          --with-tree        \
          --with-push        \
          --with-xptr        \
          --with-valid       \
          --with-xpath       \
          --with-xinclude    \
          --with-sax1        \
          --without-zlib     \
          --without-iconv    \
          --without-http     \
          --without-ftp      \
          --without-docbook  \
          --without-regexps  \
          --without-python

ifdef WITH_OPTIMIZATION
XMLCONF+= --without-debug
endif

XMLCONF += CFLAGS="$(CFLAGS) -DLIBXML_STATIC"

libxml2: libxml2-$(LIBXML2_VERSION).tar.xz .sum-libxml2
	$(UNPACK)
	$(APPLY) $(SRC)/libxml2/win32.patch
	$(APPLY) $(SRC)/libxml2/bins.patch
	$(APPLY) $(SRC)/libxml2/pthread.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/libxml2/nogetcwd.patch
endif
	$(call pkg_static,"libxml-2.0.pc.in")
	$(MOVE)

.libxml2: libxml2
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(XMLCONF)
	$(MAKE) -C $< install
	touch $@
