# ZLIB
ZLIB_VERSION := 1.3.2
ZLIB_URL := $(GITHUB)/madler/zlib/releases/download/v$(ZLIB_VERSION)/zlib-$(ZLIB_VERSION).tar.xz

PKGS += zlib
ifeq ($(call need_pkg,"zlib"),)
PKGS_FOUND += zlib
endif

$(TARBALLS)/zlib-$(ZLIB_VERSION).tar.xz:
	$(call download_pkg,$(ZLIB_URL),zlib)

.sum-zlib: zlib-$(ZLIB_VERSION).tar.xz

zlib: zlib-$(ZLIB_VERSION).tar.xz .sum-zlib
	$(UNPACK)
	$(APPLY) $(SRC)/zlib/0001-CMakeList.txt-force-static-library-name-to-z.patch
	$(MOVE)

ZLIB_CONF = -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_EXAMPLES=OFF -DZLIB_BUILD_TESTING=OFF

.zlib: zlib toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(ZLIB_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
