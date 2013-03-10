# libiconv
LIBICONV_VERSION=1.14
LIBICONV_URL=$(GNU)/libiconv/libiconv-$(LIBICONV_VERSION).tar.gz

PKGS += iconv
# iconv cannot be detect with pkg-config, but it is mandated by POSIX.
# Hard-code based on the operating system.
ifndef HAVE_WIN32
PKGS_FOUND += iconv
endif

$(TARBALLS)/libiconv-$(LIBICONV_VERSION).tar.gz:
	$(call download,$(LIBICONV_URL))

.sum-iconv: libiconv-$(LIBICONV_VERSION).tar.gz

iconv: libiconv-$(LIBICONV_VERSION).tar.gz .sum-iconv
	$(UNPACK)
ifdef HAVE_WIN64
	$(APPLY) $(SRC)/iconv/libiconv-win64.patch
endif
ifdef HAVE_WINCE
	$(APPLY) $(SRC)/iconv/libiconv-wince.patch
	$(APPLY) $(SRC)/iconv/libiconv-wince-hack.patch
endif
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/iconv/libiconv-android-ios.patch
endif
ifdef HAVE_IOS
	$(APPLY) $(SRC)/iconv/libiconv-android-ios.patch
endif
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub build-aux
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub libcharset/build-aux
	$(MOVE)

.iconv: iconv
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-nls
	cd $< && $(MAKE) install
	touch $@
