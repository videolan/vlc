# libiconv
LIBICONV_VERSION := 1.18
LIBICONV_URL := $(GNU)/libiconv/libiconv-$(LIBICONV_VERSION).tar.gz

PKGS += iconv
# iconv cannot be detect with pkg-config, but it is mandated by POSIX.
# Hard-code based on the operating system.
ifndef HAVE_WIN32
ifndef HAVE_MACOSX # in macOS 14.4 iconv seems broken (check iconv.m4 tests)
ifndef HAVE_ANDROID
PKGS_FOUND += iconv
else
ifeq ($(shell expr "$(ANDROID_API)" '>=' '28'), 1)
PKGS_FOUND += iconv
endif
endif
endif
endif

DEPS_iconv =
ifdef HAVE_WINSTORE
# gnulib uses GetFileInformationByHandle
DEPS_iconv += alloweduwp $(DEPS_alloweduwp)
endif

$(TARBALLS)/libiconv-$(LIBICONV_VERSION).tar.gz:
	$(call download_pkg,$(LIBICONV_URL),iconv)

.sum-iconv: libiconv-$(LIBICONV_VERSION).tar.gz

iconv: libiconv-$(LIBICONV_VERSION).tar.gz .sum-iconv
	$(UNPACK)
	$(call update_autoconfig,build-aux)
	$(call update_autoconfig,libcharset/build-aux)
	$(APPLY) $(SRC)/iconv/bins.patch

	# use CreateFile2 in Win8 as CreateFileW is forbidden in UWP
	$(APPLY) $(SRC)/iconv/0001-Use-CreateFile2-in-UWP-builds.patch

	$(MOVE)

ICONV_CONF := --disable-nls

.iconv: iconv
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(ICONV_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
