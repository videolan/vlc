# ZLIB
ZLIB_VERSION := 1.3.1
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
	# disable the installation of the dynamic library since there's no option
	sed -e 's,install(TARGETS zlib zlibstatic,install(TARGETS zlibstatic,' -i.orig $(UNPACK_DIR)/CMakeLists.txt
	# only use the proper libz name for the static library
	sed -e 's,set_target_properties(zlib zlibstatic ,set_target_properties(zlibstatic ,' -i.orig $(UNPACK_DIR)/CMakeLists.txt
	# don't use --version-script on static libraries
	sed -e 's,if(NOT APPLE AND NOT(CMAKE_SYSTEM_NAME STREQUAL AIX)),if(BUILD_SHARED_LIBS AND (NOT APPLE AND NOT(CMAKE_SYSTEM_NAME STREQUAL AIX))),' -i.orig $(UNPACK_DIR)/CMakeLists.txt
	$(MOVE)

ZLIB_CONF = -DINSTALL_PKGCONFIG_DIR:STRING=$(PREFIX)/lib/pkgconfig -DZLIB_BUILD_EXAMPLES=OFF

.zlib: zlib toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(ZLIB_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
