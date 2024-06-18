# LIBARCHIVE
LIBARCHIVE_VERSION := 3.6.2
LIBARCHIVE_URL := http://www.libarchive.org/downloads/libarchive-$(LIBARCHIVE_VERSION).tar.gz

PKGS += libarchive
ifeq ($(call need_pkg,"libarchive >= 3.2.0"),)
PKGS_FOUND += libarchive
endif

DEPS_libarchive = zlib $(DEPS_zlib)

LIBARCHIVE_CONF := \
		-DENABLE_CPIO=OFF -DENABLE_TAR=OFF -DENABLE_CAT=OFF \
		-DENABLE_NETTLE=OFF \
		-DENABLE_LIBXML2=OFF -DENABLE_LZMA=OFF -DENABLE_ICONV=OFF -DENABLE_EXPAT=OFF \
		-DENABLE_TEST=OFF

# CNG enables bcrypt on Windows and useless otherwise, it's not used when building for XP
LIBARCHIVE_CONF +=-DENABLE_CNG=ON

ifdef HAVE_WIN32
LIBARCHIVE_CONF += -DENABLE_OPENSSL=OFF
endif

$(TARBALLS)/libarchive-$(LIBARCHIVE_VERSION).tar.gz:
	$(call download_pkg,$(LIBARCHIVE_URL),libarchive)

.sum-libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz

libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz .sum-libarchive
	$(UNPACK)
	$(APPLY) $(SRC)/libarchive/0001-Fix-compile-on-Android.patch
	$(APPLY) $(SRC)/libarchive/0001-Fix-build-error-when-cross-compiling-for-Windows.patch
	$(APPLY) $(SRC)/libarchive/0001-Fix-bcrypt-detection-on-UNIX-cross-compilation.patch
	$(APPLY) $(SRC)/libarchive/0001-Use-the-common-CMake-BUILD_SHARED_LIBS-to-build-shar.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/libarchive/winrt.patch
endif
	$(APPLY) $(SRC)/libarchive/0001-Fix-usage-of-GetVolumePathNameW-in-UWP-before-20H1.patch
	$(call pkg_static,"build/pkgconfig/libarchive.pc.in")
	$(MOVE)

.libarchive: libarchive toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(LIBARCHIVE_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
