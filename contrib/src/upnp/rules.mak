# UPNP
UPNP_VERSION := 1.14.20
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

ifdef HAVE_WIN32
DEPS_upnp += winpthreads $(DEPS_winpthreads)
endif

UPNP_CONF := -DUPNP_BUILD_SHARED=OFF \
	-DBUILD_TESTING=OFF \
	-DUPNP_BUILD_SAMPLES=OFF

ifdef HAVE_IOS
UPNP_CONF += -DUPNP_ENABLE_IPV6=OFF -DUPNP_ENABLE_UNSPECIFIED_SERVER=ON \
 -DUPNP_MINISERVER_REUSEADDR=OFF
else
UPNP_CONF += -DUPNP_ENABLE_IPV6=ON
endif

upnp: pupnp-release-$(UPNP_VERSION).tar.gz .sum-upnp
	$(UNPACK)
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/upnp/revert-ifaddrs.patch
else
	# Avoid forcing `-lpthread` on android as it does not provide it and
	# identifies as 'Linux' in CMake.
	$(APPLY) $(SRC)/upnp/libtool-nostdlib-workaround.patch
endif
	$(APPLY) $(SRC)/upnp/miniserver.patch
ifdef HAVE_IOS
	$(APPLY) $(SRC)/upnp/fix-reuseaddr-option.patch
endif
	$(MOVE)

.upnp: upnp toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(UPNP_CONF)
	+$(CMAKEBUILD)
	+$(CMAKEINSTALL)
	touch $@
