# GnuTLS

GNUTLS_VERSION := 3.1.12
GNUTLS_URL := ftp://ftp.gnutls.org/gcrypt/gnutls/v3.1/gnutls-$(GNUTLS_VERSION).tar.xz

PKGS += gnutls
ifeq ($(call need_pkg,"gnutls >= 3.0.20"),)
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
	$(APPLY) $(SRC)/gnutls/downgrade-automake-requirement.patch
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
	$(HOSTCONF)

DEPS_gnutls = nettle $(DEPS_nettle)

.gnutls: gnutls
ifdef HAVE_ANDROID
	$(RECONF)
	cd $< && $(HOSTVARS) gl_cv_header_working_stdint_h=yes ./configure $(GNUTLS_CONF)
else
	cd $< && $(HOSTVARS) ./configure $(GNUTLS_CONF)
endif
	cd $</gl && $(MAKE) install
	cd $</lib && $(MAKE) install
	touch $@
