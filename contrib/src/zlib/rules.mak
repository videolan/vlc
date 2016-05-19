# ZLIB
ZLIB_VERSION := 1.2.8
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

ifdef HAVE_SOLARIS
ZLIB_ECFLAGS = -fPIC -DPIC
endif

$(TARBALLS)/zlib-$(ZLIB_VERSION).tar.gz:
	$(call download,$(ZLIB_URL))

.sum-zlib: zlib-$(ZLIB_VERSION).tar.gz

zlib: zlib-$(ZLIB_VERSION).tar.gz .sum-zlib
	$(UNPACK)
	$(MOVE)

.zlib: zlib
ifdef HAVE_WIN32
	cd $< && $(HOSTVARS) $(MAKE) -fwin32/Makefile.gcc $(HOSTVARS) $(ZLIB_CONFIG_VARS) CFLAGS="$(CFLAGS) $(ZLIB_ECFLAGS)" RC="$(HOST)-windres" LD="$(CC)"
	cd $< && $(MAKE) -fwin32/Makefile.gcc install INCLUDE_PATH="$(PREFIX)/include" LIBRARY_PATH="$(PREFIX)/lib" BINARY_PATH="$(PREFIX)/bin"
else
	cd $< && $(HOSTVARS) $(ZLIB_CONFIG_VARS) CFLAGS="$(CFLAGS) $(ZLIB_ECFLAGS)" ./configure --prefix=$(PREFIX) --static
	cd $< && $(MAKE) install
endif
	touch $@
