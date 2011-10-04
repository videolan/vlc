# libiconv
LIBICONV_VERSION=1.13.1
LIBICONV_URL=$(GNU)/libiconv/libiconv-$(LIBICONV_VERSION).tar.gz

ifeq ($(call need_pkg,"iconv"),)
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
	$(MOVE)

.iconv: iconv
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-nls
	cd $< && $(MAKE) install
	touch $@
