# GnuTLS

GNUTLS_VERSION := 3.5.16
GNUTLS_URL := ftp://ftp.gnutls.org/gcrypt/gnutls/v3.5/gnutls-$(GNUTLS_VERSION).tar.xz

ifdef BUILD_NETWORK
ifndef HAVE_DARWIN_OS
PKGS += gnutls
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
	$(APPLY) $(SRC)/gnutls/32b5628-upstream.patch
	$(APPLY) $(SRC)/gnutls/gnutls-pkgconfig-static.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/gnutls/gnutls-win32.patch
	$(APPLY) $(SRC)/gnutls/gnutls-loadlibrary.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/gnutls/gnutls-winrt.patch
	$(APPLY) $(SRC)/gnutls/winrt-topendir.patch
endif
endif
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/gnutls/no-create-time-h.patch
endif
	$(APPLY) $(SRC)/gnutls/read-file-limits.h.patch
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/gnutls/gnutls-disable-getentropy-osx.patch
	$(APPLY) $(SRC)/gnutls/gnutls-disable-connectx-macos.patch
endif
	$(APPLY) $(SRC)/gnutls/gnutls-libidn.patch
	$(call pkg_static,"lib/gnutls.pc.in")
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

GNUTLS_CONF := \
	--disable-gtk-doc \
	--without-p11-kit \
	--disable-cxx \
	--disable-srp-authentication \
	--disable-psk-authentication-FIXME \
	--disable-anon-authentication \
	--disable-openpgp-authentication \
	--disable-openssl-compatibility \
	--disable-guile \
	--disable-nls \
	--without-libintl-prefix \
	--disable-doc \
	--disable-tests \
	--with-included-libtasn1 \
	--with-included-unistring \
	$(HOSTCONF)

GNUTLS_ENV := $(HOSTVARS)

DEPS_gnutls = nettle $(DEPS_nettle)

ifdef HAVE_ANDROID
GNUTLS_ENV += gl_cv_header_working_stdint_h=yes
endif
ifdef HAVE_TIZEN
	GNUTLS_CONF += --with-default-trust-store-dir=/etc/ssl/certs/
endif
ifdef HAVE_WINSTORE
ifeq ($(ARCH),x86_64)
	GNUTLS_CONF += --disable-hardware-acceleration
endif
endif

.gnutls: gnutls
	$(RECONF)
	cd $< && $(GNUTLS_ENV) ./configure $(GNUTLS_CONF)
	cd $</gl && $(MAKE) install
	cd $</lib && $(MAKE) install
	touch $@
