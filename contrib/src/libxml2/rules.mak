# libxml2

LIBXML2_VERSION := 2.7.8
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
	$(MOVE)

.libxml2: libxml2
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) CFLAGS="-DLIBXML_STATIC" $(XMLCONF)
	cd $< && $(MAKE) install
	touch $@
