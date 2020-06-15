# GnuTLS

GNUTLS_VERSION := 3.6.14
GNUTLS_URL := https://www.gnupg.org/ftp/gcrypt/gnutls/v3.6/gnutls-$(GNUTLS_VERSION).tar.xz

ifdef BUILD_NETWORK
ifndef HAVE_DARWIN_OS
PKGS += gnutls
endif
endif
ifeq ($(call need_pkg,"gnutls >= 3.5.0"),)
PKGS_FOUND += gnutls
endif

$(TARBALLS)/gnutls-$(GNUTLS_VERSION).tar.xz:
	$(call download_pkg,$(GNUTLS_URL),gnutls)

.sum-gnutls: gnutls-$(GNUTLS_VERSION).tar.xz

gnutls: gnutls-$(GNUTLS_VERSION).tar.xz .sum-gnutls
	$(UNPACK)
	$(APPLY) $(SRC)/gnutls/gnutls-fix-mangling.patch

	# backport gnulib patch
	$(APPLY) $(SRC)/gnutls/0001-Don-t-assume-that-UNICODE-is-not-defined.patch

	# fix forbidden UWP call which can't be upstreamed as they won't
	# differentiate for winstore, only _WIN32_WINNT
	$(APPLY) $(SRC)/gnutls/0001-fcntl-do-not-call-GetHandleInformation-in-Winstore-a.patch

	# forbidden RtlSecureZeroMemory call in winstore builds
	$(APPLY) $(SRC)/gnutls/0001-explicit_bzero-Do-not-call-SecureZeroMemory-on-UWP-b.patch

	# disable the dllimport in static linking (pkg-config --static doesn't handle Cflags.private)
	cd $(UNPACK_DIR) && sed -i.orig -e s/"_SYM_EXPORT __declspec(dllimport)"/"_SYM_EXPORT"/g lib/includes/gnutls/gnutls.h.in

	# don't use connectx on macOS versions where it's not available
	$(APPLY) $(SRC)/gnutls/0001-fix-connectx-not-available-on-older-macOS-SDK.patch
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/gnutls/no-create-time-h.patch
endif
	$(call pkg_static,"lib/gnutls.pc.in")
	$(UPDATE_AUTOCONFIG)
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
	--with-included-unistring \
	$(HOSTCONF)

GNUTLS_ENV := $(HOSTVARS)

DEPS_gnutls = nettle $(DEPS_nettle)

ifdef HAVE_ANDROID
GNUTLS_ENV += gl_cv_header_working_stdint_h=yes
endif
ifdef HAVE_WINSTORE
ifeq ($(ARCH),x86_64)
	GNUTLS_CONF += --disable-hardware-acceleration
endif
endif
ifdef HAVE_WIN32
	GNUTLS_CONF += --without-idn
ifeq ($(ARCH),aarch64)
	# Gnutls' aarch64 assembly unconditionally uses ELF specific directives
	GNUTLS_CONF += --disable-hardware-acceleration
endif
endif

ifdef HAVE_NACL
	GNUTLS_CONF += --disable-hardware-acceleration
endif

.gnutls: gnutls
	cd $< && $(GNUTLS_ENV) ./configure $(GNUTLS_CONF)
	cd $< && $(MAKE) -C gl install
	cd $< && $(MAKE) -C lib install
	touch $@
