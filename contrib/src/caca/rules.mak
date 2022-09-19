# CACA
CACA_VERSION := 0.99.beta20
CACA_URL := $(GITHUB)/cacalabs/libcaca/releases/download/v$(CACA_VERSION)/libcaca-$(CACA_VERSION).tar.gz

ifndef HAVE_DARWIN_OS
ifndef HAVE_LINUX # see VLC Trac 17251
ifndef HAVE_WINSTORE
PKGS += caca
endif
endif
endif

ifeq ($(call need_pkg,"caca >= 0.99.beta19"),)
PKGS_FOUND += caca
endif

$(TARBALLS)/libcaca-$(CACA_VERSION).tar.gz:
	$(call download_pkg,$(CACA_URL),caca)

.sum-caca: libcaca-$(CACA_VERSION).tar.gz

caca: libcaca-$(CACA_VERSION).tar.gz .sum-caca
	$(UNPACK)
	$(APPLY) $(SRC)/caca/caca-fix-compilation-llvmgcc.patch
	$(APPLY) $(SRC)/caca/caca-fix-pkgconfig.patch
	$(call pkg_static,"caca/caca.pc.in")
	$(UPDATE_AUTOCONFIG)
	$(MOVE)
	mv caca/config.sub caca/config.guess caca/.auto

CACA_CONF := \
	--disable-gl \
	--disable-imlib2 \
	--disable-doc \
	--disable-cppunit \
	--disable-zzuf \
	--disable-ruby \
	--disable-csharp \
	--disable-cxx \
	--disable-java \
	--disable-python \
	--disable-cocoa \
	--disable-network \
	--disable-vga \
	--disable-imlib2
ifdef HAVE_MACOSX
CACA_CONF += --disable-x11
endif
ifndef WITH_OPTIMIZATION
CACA_CONF += --enable-debug
endif
ifdef HAVE_WIN32
CACA_CONF += --disable-ncurses \
    ac_cv_func_vsnprintf_s=yes \
    ac_cv_func_sprintf_s=yes
endif
ifdef HAVE_LINUX
CACA_CONF += --disable-ncurses
endif

CACA_CONF += \
	MACOSX_SDK=$(MACOSX_SDK) \
	MACOSX_SDK_CFLAGS=" " \
	MACOSX_SDK_CXXFLAGS=" " \
	CPPFLAGS="$(CPPFLAGS) -DCACA_STATIC"

.caca: caca
	mkdir -p $</_build
	cd $</_build && $(HOSTVARS) ../configure $(HOSTCONF) $(CACA_CONF)
	$(MAKE) -C $</_build -C $< install
	touch $@
