# UPNP
UPNP_VERSION := 1.14.11
UPNP_URL := $(GITHUB)/pupnp/pupnp/archive/refs/tags/release-$(UPNP_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += upnp
endif
ifeq ($(call need_pkg,"libupnp >= 1.8.3"),)
PKGS_FOUND += upnp
endif

$(TARBALLS)/pupnp-release-$(UPNP_VERSION).tar.gz:
	$(call download_pkg,$(UPNP_URL),upnp)

.sum-upnp: pupnp-release-$(UPNP_VERSION).tar.gz

UPNP_CFLAGS := $(CFLAGS) -DUPNP_STATIC_LIB
UPNP_CXXFLAGS := $(CXXFLAGS) -DUPNP_STATIC_LIB
UPNP_CONF := --disable-samples

ifdef HAVE_WIN32
DEPS_upnp += pthreads $(DEPS_pthreads)
endif
ifdef HAVE_WINSTORE
UPNP_CONF += --disable-ipv6 --enable-unspecified_server
else
ifdef HAVE_IOS
UPNP_CONF += --disable-ipv6 --enable-unspecified_server
else
UPNP_CONF += --enable-ipv6
endif
endif
ifndef WITH_OPTIMIZATION
UPNP_CONF += --enable-debug
endif

upnp: pupnp-release-$(UPNP_VERSION).tar.gz .sum-upnp
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/upnp/libupnp-pthread-force.patch
	$(APPLY) $(SRC)/upnp/libupnp-win32-exports.patch
	$(APPLY) $(SRC)/upnp/libupnp-win32.patch
	$(APPLY) $(SRC)/upnp/libupnp-win64.patch
	$(APPLY) $(SRC)/upnp/windows-version-inet.patch
	$(APPLY) $(SRC)/upnp/win32-gettimeofday.patch
endif
ifdef HAVE_LINUX
ifndef HAVE_ANDROID
	$(APPLY) $(SRC)/upnp/libupnp-pthread-force.patch
endif
endif
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/upnp/revert-ifaddrs.patch
endif
	$(APPLY) $(SRC)/upnp/miniserver.patch
	$(APPLY) $(SRC)/upnp/miniserver-pton-error.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.upnp: upnp
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(UPNP_CFLAGS)" CXXFLAGS="$(UPNP_CXXFLAGS)" ./configure $(UPNP_CONF) $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
