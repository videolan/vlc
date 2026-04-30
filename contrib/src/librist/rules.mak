# librist

LIBRIST_VERSION := v0.2.14
LIBRIST_URL := https://code.videolan.org/rist/librist/-/archive/$(LIBRIST_VERSION)/librist-$(LIBRIST_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += librist
endif

DEPS_librist =
ifdef HAVE_WIN32
DEPS_librist += winpthreads $(DEPS_winpthreads)
endif
ifdef HAVE_WINSTORE
# librist uses wincrypt
DEPS_librist += alloweduwp $(DEPS_alloweduwp)
endif

ifeq ($(call need_pkg,"librist >= 0.2"),)
PKGS_FOUND += librist
endif

LIBRIST_CONF = -Dbuilt_tools=false -Dtest=false
ifdef HAVE_WIN32
LIBRIST_CONF += -Dhave_mingw_pthreads=true
endif

# Prefer nettle+gmp+gnutls over the bundled mbedtls when the license
# allows it. gnutls (nettle/gmp) can't be used with the LGPLv2 license.
ifdef GPL
LIBRIST_USE_GNUTLS = 1
else
ifdef GNUV3
LIBRIST_USE_GNUTLS = 1
endif
endif

ifeq ($(LIBRIST_USE_GNUTLS),1)
DEPS_librist += gnutls $(DEPS_gnutls)
LIBRIST_CONF += -Duse_nettle=true -Duse_mbedtls=false
endif

$(TARBALLS)/librist-$(LIBRIST_VERSION).tar.gz:
	$(call download_pkg,$(LIBRIST_URL),librist)

.sum-librist: librist-$(LIBRIST_VERSION).tar.gz

librist: librist-$(LIBRIST_VERSION).tar.gz .sum-librist
	$(UNPACK)
	$(MOVE)

.librist: librist crossfile.meson
	$(MESONCLEAN)
	$(MESON) $(LIBRIST_CONF)
	+$(MESONBUILD)
	touch $@
