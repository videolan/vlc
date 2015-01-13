# GnuTLS

GNUTLS_VERSION := 3.2.21
GNUTLS_URL := ftp://ftp.gnutls.org/gcrypt/gnutls/v3.2/gnutls-$(GNUTLS_VERSION).tar.xz

ifdef BUILD_NETWORK
ifndef HAVE_DARWIN_OS
PKGS += gnutls
endif
endif
ifeq ($(call need_pkg,"gnutls >= 3.2.0"),)
PKGS_FOUND += gnutls
endif

$(TARBALLS)/gnutls-$(GNUTLS_VERSION).tar.xz:
	$(call download,$(GNUTLS_URL))

.sum-gnutls: gnutls-$(GNUTLS_VERSION).tar.xz

gnutls: gnutls-$(GNUTLS_VERSION).tar.xz .sum-gnutls
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/gnutls/gnutls-win32.patch
endif
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/gnutls/no-create-time-h.patch
endif
	$(APPLY) $(SRC)/gnutls/gnutls-no-egd.patch
	$(APPLY) $(SRC)/gnutls/read-file-limits.h.patch
	$(APPLY) $(SRC)/gnutls/mac-keychain-lookup.patch
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/gnutls/gnutls-pkgconfig-osx.patch
endif
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
	$(HOSTCONF)

DEPS_gnutls = nettle $(DEPS_nettle)

.gnutls: gnutls
	$(RECONF)
ifdef HAVE_ANDROID
	cd $< && $(HOSTVARS) gl_cv_header_working_stdint_h=yes ./configure $(GNUTLS_CONF)
else
	cd $< && $(HOSTVARS) ./configure $(GNUTLS_CONF)
endif
	cd $</gl && $(MAKE) install
	cd $</lib && $(MAKE) install
	touch $@
