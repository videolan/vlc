# zvbi

ZVBI_VERSION := 0.2.33
ZVBI_URL := $(SF)/zapping/zvbi-$(ZVBI_VERSION).tar.bz2

PKGS += zvbi
ifeq ($(call need_pkg,"zvbi-0.2"),)
PKGS_FOUND += zvbi
endif

$(TARBALLS)/zvbi-$(ZVBI_VERSION).tar.bz2:
	$(call download,$(ZVBI_URL))

.sum-zvbi: zvbi-$(ZVBI_VERSION).tar.bz2

zvbi: zvbi-$(ZVBI_VERSION).tar.bz2 .sum-zvbi
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/zvbi/zvbi-win32.patch
endif
	$(MOVE)

DEPS_zvbi = pthreads iconv $(DEPS_iconv)

ZVBI_CFLAGS := $(CFLAGS)
ZVBICONF := \
	--disable-v4l --disable-dvb --disable-bktr \
	--disable-nls --disable-proxy \
	--without-doxygen \
	$(HOSTCONF)
ifdef HAVE_MACOSX
ZVBI_CFLAGS += -fnested-functions
endif
ifdef HAVE_WIN32
ZVBI_CFLAGS += -DPTW32_STATIC_LIB
endif

.zvbi: zvbi
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(ZVBI_CFLAGS)" ./configure $(ZVBICONF)
	cd $</src && $(MAKE) install
	cd $< && $(MAKE) SUBDIRS=. install
	touch $@
