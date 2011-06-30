# ASS
ASS_VERSION := 0.9.12
ASS_URL := http://libass.googlecode.com/files/libass-$(ASS_VERSION).tar.gz

PKGS += ass

$(TARBALLS)/libass-$(ASS_VERSION).tar.gz:
	$(call download,$(ASS_URL))

.sum-ass: libass-$(ASS_VERSION).tar.gz

libass: libass-$(ASS_VERSION).tar.gz .sum-ass
	$(UNPACK)
	$(MOVE)

#TODO .fontconfig

.ass: libass .freetype2
	#cd $< && autoreconf -fiv $(ACLOCAL_AMFLAGS)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -O3" ./configure $(HOSTCONF) --disable-png --disable-enca
	cd $< && $(MAKE) install
	touch $@
