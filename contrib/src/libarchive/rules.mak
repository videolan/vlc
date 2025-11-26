# LIBARCHIVE
LIBARCHIVE_VERSION := 3.8.3
LIBARCHIVE_URL := $(GITHUB)/libarchive/libarchive/releases/download/v$(LIBARCHIVE_VERSION)/libarchive-$(LIBARCHIVE_VERSION).tar.gz

PKGS += libarchive
ifeq ($(call need_pkg,"libarchive >= 3.2.0"),)
PKGS_FOUND += libarchive
endif

DEPS_libarchive = zlib $(DEPS_zlib)
ifdef HAVE_WINSTORE
# libarchive uses CreateHardLinkW and wincrypt
DEPS_libarchive += alloweduwp $(DEPS_alloweduwp)
endif

LIBARCHIVE_CONF := \
		-DENABLE_CPIO=OFF -DENABLE_TAR=OFF -DENABLE_CAT=OFF \
		-DENABLE_NETTLE=OFF \
		-DENABLE_LIBXML2=OFF -DENABLE_LZMA=OFF -DENABLE_ICONV=OFF -DENABLE_EXPAT=OFF \
		-DENABLE_TEST=OFF -DENABLE_WERROR=OFF

# CNG enables bcrypt on Windows and useless otherwise, it's OK we build for Win7+
LIBARCHIVE_CONF +=-DENABLE_CNG=ON

# bsdunzip doesn't build on macos, android and emscripten and it's disabled on Windows
LIBARCHIVE_CONF +=-DENABLE_UNZIP=OFF

ifdef HAVE_WIN32
LIBARCHIVE_CONF += -DENABLE_OPENSSL=OFF
endif

$(TARBALLS)/libarchive-$(LIBARCHIVE_VERSION).tar.gz:
	$(call download_pkg,$(LIBARCHIVE_URL),libarchive)

.sum-libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz

libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz .sum-libarchive
	$(UNPACK)
	$(APPLY) $(SRC)/libarchive/0001-zstd-use-GetNativeSystemInfo-to-get-the-number-of-th.patch
	$(call pkg_static,"build/pkgconfig/libarchive.pc.in")
	$(MOVE)

.libarchive: libarchive toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(LIBARCHIVE_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
