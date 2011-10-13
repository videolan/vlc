# CACA
CACA_VERSION := 0.99.beta17
CACA_URL := http://caca.zoy.org/files/libcaca/libcaca-$(CACA_VERSION).tar.gz

PKGS += caca
ifeq ($(call need_pkg,"caca >= 0.99.beta14"),)
PKGS_FOUND += caca
endif

$(TARBALLS)/libcaca-$(CACA_VERSION).tar.gz:
	$(call download,$(CACA_URL))

.sum-caca: libcaca-$(CACA_VERSION).tar.gz

caca: libcaca-$(CACA_VERSION).tar.gz .sum-caca
	$(UNPACK)
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/caca/caca-osx-sdkofourchoice.patch
endif
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/caca/caca-win32-static.patch
endif
	$(MOVE)

.caca: caca
ifdef HAVE_MACOSX
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-imlib2 --disable-doc --disable-ruby --disable-csharp --disable-cxx --disable-x11
else
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-imlib2 --disable-doc --disable-ruby --disable-csharp --disable-cxx
endif
	cd $< && $(MAKE) -C $< install
	touch $@
