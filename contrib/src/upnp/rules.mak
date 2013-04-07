# UPNP
UPNP_VERSION := 1.6.18
UPNP_URL := $(SF)/pupnp/libupnp-$(UPNP_VERSION).tar.bz2

PKGS += upnp

$(TARBALLS)/libupnp-$(UPNP_VERSION).tar.bz2:
	$(call download,$(UPNP_URL))

.sum-upnp: libupnp-$(UPNP_VERSION).tar.bz2

ifdef HAVE_WIN32
DEPS_upnp += pthreads $(DEPS_pthreads)
LIBUPNP_ECFLAGS = -DPTW32_STATIC_LIB
endif

upnp: libupnp-$(UPNP_VERSION).tar.bz2 .sum-upnp
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/upnp/libupnp-configure.patch
	$(APPLY) $(SRC)/upnp/libupnp-win32.patch
	$(APPLY) $(SRC)/upnp/libupnp-win64.patch
endif
	$(APPLY) $(SRC)/upnp/libupnp-ipv6.patch
	$(APPLY) $(SRC)/upnp/miniserver.patch
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub build-aux/
	$(MOVE)

.upnp: upnp
ifdef HAVE_WIN32
	$(RECONF)
endif
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DUPNP_STATIC_LIB $(LIBUPNP_ECFLAGS)" ./configure --disable-samples --without-documentation --enable-ipv6 $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
