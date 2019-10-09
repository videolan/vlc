# CACA
CACA_VERSION := 0.99.beta17
CACA_URL := http://caca.zoy.org/files/libcaca/libcaca-$(CACA_VERSION).tar.gz

ifndef HAVE_LINUX # see VLC Trac 17251
ifndef HAVE_WINSTORE
PKGS += caca
endif
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
	$(APPLY) $(SRC)/caca/caca-fix-ln-call.patch
	$(APPLY) $(SRC)/caca/caca-fix-pkgconfig.patch
	$(call pkg_static,"caca/caca.pc.in")
	$(UPDATE_AUTOCONFIG)
	$(MOVE)
	mv caca/config.sub caca/config.guess caca/.auto

CACA_CONF := \
	--disable-gl \
	--disable-imlib2 \
	--disable-doc \
	--disable-ruby \
	--disable-csharp \
	--disable-cxx \
	--disable-java
ifdef HAVE_MACOSX
CACA_CONF += --disable-x11
endif
ifdef HAVE_WIN32
CACA_CONF += --disable-ncurses
endif
ifdef HAVE_LINUX
CACA_CONF += --disable-ncurses
endif

.caca: caca
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(CACA_CONF)
	cd $< && $(MAKE) -C $< install
	touch $@
