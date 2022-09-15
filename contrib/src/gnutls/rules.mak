# GnuTLS

GNUTLS_MAJVERSION := 3.8
GNUTLS_VERSION := $(GNUTLS_MAJVERSION).3
GNUTLS_URL := $(GNUGPG)/gnutls/v$(GNUTLS_MAJVERSION)/gnutls-$(GNUTLS_VERSION).tar.xz

# nettle/gmp can't be used with the LGPLv2 license
ifdef GPL
GNUTLS_PKG=1
else
ifdef GNUV3
GNUTLS_PKG=1
endif
endif

ifdef BUILD_NETWORK
ifndef HAVE_DARWIN_OS
ifdef GNUTLS_PKG
PKGS += gnutls
endif
endif
endif
ifeq ($(call need_pkg,"gnutls >= 3.3.6"),)
PKGS_FOUND += gnutls
endif

$(TARBALLS)/gnutls-$(GNUTLS_VERSION).tar.xz:
	$(call download_pkg,$(GNUTLS_URL),gnutls)

.sum-gnutls: gnutls-$(GNUTLS_VERSION).tar.xz

gnutls: gnutls-$(GNUTLS_VERSION).tar.xz .sum-gnutls
	$(UNPACK)
	# fix forbidden UWP call which can't be upstreamed as they won't
	# differentiate for winstore, only _WIN32_WINNT
	$(APPLY) $(SRC)/gnutls/0001-fcntl-do-not-call-GetHandleInformation-in-Winstore-a.patch

	# disable the dllimport in static linking (pkg-config --static doesn't handle Cflags.private)
	sed -i.orig -e s/"_SYM_EXPORT __declspec(dllimport)"/"_SYM_EXPORT"/g $(UNPACK_DIR)/lib/includes/gnutls/gnutls.h.in

	# disable __faccessat usage on Darwin as it's not available on our minimum target
	$(APPLY) $(SRC)/gnutls/__faccessat-darwin.patch

	# replace HANDLE_FLAG_INHERIT which may not be available in older UWP
	sed -i.orig -e s/HANDLE_FLAG_INHERIT/0x1/g $(UNPACK_DIR)/gl/fcntl.c

	$(call pkg_static,"lib/gnutls.pc.in")

	# use CreateFile2 in Win8 as CreateFileW is forbidden in UWP
	$(APPLY) $(SRC)/gnutls/0001-Use-CreateFile2-in-UWP-builds.patch

	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub build-aux
	$(MOVE)

GNUTLS_CONF := \
	--disable-gtk-doc \
	--without-p11-kit \
	--disable-cxx \
	--disable-srp-authentication \
	--disable-anon-authentication \
	--disable-openssl-compatibility \
	--disable-guile \
	--disable-nls \
	--without-libintl-prefix \
	--disable-doc \
	--disable-tools \
	--disable-tests \
	--with-included-libtasn1 \
	--with-included-unistring

DEPS_gnutls = nettle $(DEPS_nettle)
ifdef HAVE_WINSTORE
# gnulib uses GetFileInformationByHandle / SecureZeroMemory
DEPS_gnutls += alloweduwp $(DEPS_alloweduwp)
endif

ifdef HAVE_ANDROID
GNUTLS_ENV := gl_cv_header_working_stdint_h=yes
endif
ifdef HAVE_WIN32
	GNUTLS_CONF += --without-idn
ifeq ($(ARCH),aarch64)
	# Gnutls' aarch64 assembly unconditionally uses ELF specific directives
	GNUTLS_CONF += --disable-hardware-acceleration
endif
endif

.gnutls: gnutls
	$(GNUTLS_ENV) cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(GNUTLS_CONF)
	$(MAKE) -C $< -C gl install
	$(MAKE) -C $< -C lib install
	touch $@
