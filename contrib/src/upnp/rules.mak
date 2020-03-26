# UPNP
UPNP_VERSION := 1.6.19
UPNP_URL := $(SF)/pupnp/libupnp-$(UPNP_VERSION).tar.bz2

ifdef BUILD_NETWORK
PKGS += upnp
endif
ifeq ($(call need_pkg,"libupnp >= 1.6.18"),)
PKGS_FOUND += upnp
endif

$(TARBALLS)/libupnp-$(UPNP_VERSION).tar.bz2:
	$(call download_pkg,$(UPNP_URL),upnp)

.sum-upnp: libupnp-$(UPNP_VERSION).tar.bz2

UPNP_CFLAGS   := $(CFLAGS)   -DUPNP_STATIC_LIB
UPNP_CXXFLAGS := $(CXXFLAGS) -DUPNP_STATIC_LIB
ifdef HAVE_WIN32
DEPS_upnp += pthreads $(DEPS_pthreads)
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
	$(APPLY) $(SRC)/upnp/libupnp-win32.patch
	$(APPLY) $(SRC)/upnp/libupnp-win64.patch
	$(APPLY) $(SRC)/upnp/windows-random.patch
	$(APPLY) $(SRC)/upnp/windows-version-inet.patch
	$(APPLY) $(SRC)/upnp/libupnp-win32-exports.patch
	$(APPLY) $(SRC)/upnp/libupnp-pthread-w32-checks.patch
	$(APPLY) $(SRC)/upnp/libupnp-pthread-w32-force.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/upnp/no-getifinfo.patch
endif
endif
	$(APPLY) $(SRC)/upnp/libpthread.patch
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
	cd $< && $(HOSTVARS) CFLAGS="$(UPNP_CFLAGS)" CXXFLAGS="$(UPNP_CXXFLAGS)" ./configure --disable-samples --without-documentation $(CONFIGURE_ARGS) $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
