# LIBARCHIVE
LIBARCHIVE_VERSION := 3.6.1
LIBARCHIVE_URL := $(GITHUB)/libarchive/libarchive/releases/download/v$(LIBARCHIVE_VERSION)/libarchive-$(LIBARCHIVE_VERSION).tar.gz

PKGS += libarchive
ifeq ($(call need_pkg,"libarchive >= 3.2.0"),)
PKGS_FOUND += libarchive
endif

DEPS_libarchive = zlib $(DEPS_zlib)
ifdef HAVE_WINSTORE
# libarchive uses CreateHardLinkW
DEPS_libarchive += alloweduwp $(DEPS_alloweduwp)
endif

LIBARCHIVE_CONF := \
		--disable-bsdcpio --disable-bsdtar --disable-bsdcat \
		--without-nettle \
		--without-xml2 --without-lzma --without-iconv --without-expat

ifdef HAVE_WIN32
# CNG enables bcrypt on Windows and useless otherwise, it's OK we build for Win7+
LIBARCHIVE_CONF += --without-openssl --with-cng
endif

$(TARBALLS)/libarchive-$(LIBARCHIVE_VERSION).tar.gz:
	$(call download_pkg,$(LIBARCHIVE_URL),libarchive)

.sum-libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz

libarchive: libarchive-$(LIBARCHIVE_VERSION).tar.gz .sum-libarchive
	$(UNPACK)
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/libarchive/android.patch
endif
	$(APPLY) $(SRC)/libarchive/0001-Use-CreateHardLinkW-and-CreateSymbolicLinkW-directly.patch
	$(APPLY) $(SRC)/libarchive/0002-Disable-CreateSymbolicLinkW-use-in-UWP-builds.patch
	$(APPLY) $(SRC)/libarchive/0003-fix-the-CreateHardLinkW-signature-to-match-the-real-.patch
	$(APPLY) $(SRC)/libarchive/0004-Don-t-call-GetOEMCP-in-Universal-Windows-Platform-bu.patch
	$(APPLY) $(SRC)/libarchive/0005-tests-use-CreateFileA-for-char-filenames.patch
	$(APPLY) $(SRC)/libarchive/0006-Use-CreateFile2-instead-of-CreateFileW-on-Win8-build.patch
	$(APPLY) $(SRC)/libarchive/0007-Disable-CreateFileA-calls-in-UWP-builds.patch
	$(APPLY) $(SRC)/libarchive/0008-Disable-program-call-with-stdin-stdout-usage-on-UWP-.patch
	$(APPLY) $(SRC)/libarchive/0009-Use-Windows-bcrypt-when-enabled-and-building-for-Vis.patch
	$(call pkg_static,"build/pkgconfig/libarchive.pc.in")
	$(MOVE)

.libarchive: libarchive
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(LIBARCHIVE_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
