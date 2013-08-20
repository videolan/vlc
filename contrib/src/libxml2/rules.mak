# libxml2

LIBXML2_VERSION := 2.9.1
LIBXML2_URL := http://xmlsoft.org/sources/libxml2-$(LIBXML2_VERSION).tar.gz

PKGS += libxml2
ifeq ($(call need_pkg,"libxml-2.0"),)
PKGS_FOUND += libxml2
endif

$(TARBALLS)/libxml2-$(LIBXML2_VERSION).tar.gz:
	$(call download,$(LIBXML2_URL))

.sum-libxml2: libxml2-$(LIBXML2_VERSION).tar.gz

XMLCONF = --with-minimal --with-catalog --with-reader --with-tree --with-push --with-xptr --with-valid --with-xpath --with-xinclude --with-sax1 --without-zlib --without-iconv --without-http --without-ftp  --without-debug --without-docbook --without-regexps --without-python

libxml2: libxml2-$(LIBXML2_VERSION).tar.gz .sum-libxml2
	$(UNPACK)
	$(APPLY) $(SRC)/libxml2/no-tests.patch
	$(APPLY) $(SRC)/libxml2/win32.patch
	$(APPLY) $(SRC)/libxml2/bins.patch
	$(APPLY) $(SRC)/libxml2/pthread.patch
	$(MOVE)

.libxml2: libxml2
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) CFLAGS="-DLIBXML_STATIC" $(XMLCONF)
	cd $< && $(MAKE) install
	touch $@
