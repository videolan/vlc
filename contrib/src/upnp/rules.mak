# UPNP
UPNP_VERSION := 1.6.19
UPNP_URL := $(SF)/pupnp/libupnp-$(UPNP_VERSION).tar.bz2

ifdef BUILD_NETWORK
PKGS += upnp
endif

$(TARBALLS)/libupnp-$(UPNP_VERSION).tar.bz2:
	$(call download_pkg,$(UPNP_URL),upnp)

.sum-upnp: libupnp-$(UPNP_VERSION).tar.bz2

ifdef HAVE_WIN32
DEPS_upnp += pthreads $(DEPS_pthreads)
LIBUPNP_ECFLAGS = -DPTW32_STATIC_LIB
endif
ifdef HAVE_WINSTORE
CONFIGURE_ARGS=--disable-ipv6 --enable-unspecified_server
else
CONFIGURE_ARGS=--enable-ipv6
endif
ifndef WITH_OPTIMIZATION
CONFIGURE_ARGS += --enable-debug
endif

upnp: libupnp-$(UPNP_VERSION).tar.bz2 .sum-upnp
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/upnp/libupnp-configure.patch
	$(APPLY) $(SRC)/upnp/libupnp-win32.patch
	$(APPLY) $(SRC)/upnp/libupnp-win64.patch
	$(APPLY) $(SRC)/upnp/windows-random.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/upnp/winrt-dont-force-win32-winnt.patch
	$(APPLY) $(SRC)/upnp/no-getifinfo.patch
	$(APPLY) $(SRC)/upnp/winrt-inet.patch
endif
endif
	$(APPLY) $(SRC)/upnp/libpthread.patch
	$(APPLY) $(SRC)/upnp/libupnp-ipv6.patch
	$(APPLY) $(SRC)/upnp/miniserver.patch
	$(APPLY) $(SRC)/upnp/missing_win32.patch
	$(APPLY) $(SRC)/upnp/fix_infinite_loop.patch
	$(APPLY) $(SRC)/upnp/dont_use_down_intf.patch
	$(APPLY) $(SRC)/upnp/upnp-no-debugfile.patch
	$(APPLY) $(SRC)/upnp/use-unicode.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub build-aux/
	$(MOVE)

.upnp: upnp
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DUPNP_STATIC_LIB $(LIBUPNP_ECFLAGS)" ./configure --disable-samples --without-documentation $(CONFIGURE_ARGS) $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
