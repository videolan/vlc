# ZLIB
ZLIB_VERSION := 1.2.11
ZLIB_URL := $(SF)/libpng/zlib-$(ZLIB_VERSION).tar.gz

PKGS += zlib
ifeq ($(call need_pkg,"zlib"),)
PKGS_FOUND += zlib
endif

ifeq ($(shell uname),Darwin) # zlib tries to use libtool on Darwin
ifdef HAVE_CROSS_COMPILE
ZLIB_CONFIG_VARS=CHOST=$(HOST)
endif
endif

$(TARBALLS)/zlib-$(ZLIB_VERSION).tar.gz:
	$(call download_pkg,$(ZLIB_URL),zlib)

.sum-zlib: zlib-$(ZLIB_VERSION).tar.gz

zlib: zlib-$(ZLIB_VERSION).tar.gz .sum-zlib
	$(UNPACK)
	$(APPLY) $(SRC)/zlib/no-shared.patch
	$(MOVE)

.zlib: zlib
ifdef HAVE_WIN32
	cd $< && $(HOSTVARS) $(MAKE) -fwin32/Makefile.gcc install $(HOSTVARS) $(ZLIB_CONFIG_VARS) LD="$(CC)" INCLUDE_PATH="$(PREFIX)/include" LIBRARY_PATH="$(PREFIX)/lib" BINARY_PATH="$(PREFIX)/bin"
else
	cd $< && $(HOSTVARS_PIC) $(ZLIB_CONFIG_VARS) ./configure --prefix=$(PREFIX) --static
	cd $< && $(MAKE) install
endif
	touch $@
