# LIBARCHIVE
LIBARCHIVE_VERSION := 3.6.2
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
		-DENABLE_TEST=OFF

# CNG enables bcrypt on Windows and useless otherwise, it's OK we build for Win7+
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
	$(APPLY) $(SRC)/libarchive/0001-Use-CreateHardLinkW-and-CreateSymbolicLinkW-directly.patch
	$(APPLY) $(SRC)/libarchive/0002-Disable-CreateSymbolicLinkW-use-in-UWP-builds.patch
	$(APPLY) $(SRC)/libarchive/0003-fix-the-CreateHardLinkW-signature-to-match-the-real-.patch
	$(APPLY) $(SRC)/libarchive/0004-Don-t-call-GetOEMCP-in-Universal-Windows-Platform-bu.patch
	$(APPLY) $(SRC)/libarchive/0005-tests-use-CreateFileA-for-char-filenames.patch
	$(APPLY) $(SRC)/libarchive/0006-Use-CreateFile2-instead-of-CreateFileW-on-Win8-build.patch
	$(APPLY) $(SRC)/libarchive/0007-Disable-CreateFileA-calls-in-UWP-builds.patch
	$(APPLY) $(SRC)/libarchive/0008-Disable-program-call-with-stdin-stdout-usage-on-UWP-.patch
	$(APPLY) $(SRC)/libarchive/0001-Make-single-bit-bitfields-unsigned-to-avoid-clang-16.patch
	$(call pkg_static,"build/pkgconfig/libarchive.pc.in")
	$(MOVE)

.libarchive: libarchive toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(LIBARCHIVE_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
