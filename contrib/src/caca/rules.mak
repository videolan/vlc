# CACA
CACA_VERSION := 0.99.beta17
CACA_URL := http://caca.zoy.org/files/libcaca/libcaca-$(CACA_VERSION).tar.gz

ifndef HAVE_LINUX # see VLC Trac 17251
PKGS += caca
endif
ifeq ($(call need_pkg,"caca >= 0.99.beta14"),)
PKGS_FOUND += caca
endif

$(TARBALLS)/libcaca-$(CACA_VERSION).tar.gz:
	$(call download_pkg,$(CACA_URL),caca)

.sum-caca: libcaca-$(CACA_VERSION).tar.gz

caca: libcaca-$(CACA_VERSION).tar.gz .sum-caca
	$(UNPACK)
	$(APPLY) $(SRC)/caca/caca-fix-compilation-llvmgcc.patch
	$(APPLY) $(SRC)/caca/caca-llvm-weak-alias.patch
	$(APPLY) $(SRC)/caca/caca-osx-sdkofourchoice.patch
	$(APPLY) $(SRC)/caca/caca-win32-static.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)
	mv caca/config.sub caca/config.guess caca/.auto

CACA_CONF := \
	--disable-imlib2 \
	--disable-doc \
	--disable-ruby \
	--disable-csharp \
	--disable-cxx \
	--disable-java
ifdef HAVE_MACOSX
CACA_CONF += --disable-x11
endif

.caca: caca
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(CACA_CONF)
	cd $< && $(MAKE) -C $< install
	touch $@
