# GnuTLS

GNUTLS_VERSION := 2.12.20
GNUTLS_URL := http://ftp.gnu.org/pub/gnu/gnutls/gnutls-$(GNUTLS_VERSION).tar.bz2

PKGS += gnutls
ifeq ($(call need_pkg,"gnutls >= 2.0.0"),)
PKGS_FOUND += gnutls
endif

$(TARBALLS)/gnutls-$(GNUTLS_VERSION).tar.bz2:
	$(call download,$(GNUTLS_URL))

.sum-gnutls: gnutls-$(GNUTLS_VERSION).tar.bz2

gnutls: gnutls-$(GNUTLS_VERSION).tar.bz2 .sum-gnutls
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/gnutls/gnutls-win32.patch
endif
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/gnutls/no-create-time-h.patch
endif
	$(APPLY) $(SRC)/gnutls/gnutls-no-egd.patch
	$(APPLY) $(SRC)/gnutls/read-file-limits.h.patch
	$(call pkg_static,"lib/gnutls.pc.in")
	$(call pkg_static,"libextra/gnutls-extra.pc.in")
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

GNUTLS_CONF := \
	--disable-gtk-doc \
	--without-p11-kit \
	--disable-cxx \
	--disable-srp-authentication \
	--disable-psk-authentication-FIXME \
	--disable-anon-authentication \
	--disable-camellia \
	--disable-openpgp-authentication \
	--disable-session-ticket \
	--disable-openssl-compatibility \
	--disable-guile \
	$(HOSTCONF)

USE_GCRYPT=0
ifdef HAVE_WIN32
USE_GCRYPT=1
endif
ifdef HAVE_MACOSX
USE_GCRYPT=1
endif
ifdef HAVE_ANDROID
USE_GCRYPT=1
endif

ifeq (1,$(USE_GCRYPT))
GNUTLS_CONF += --with-libgcrypt
DEPS_gnutls = gcrypt $(DEPS_gcrypt)
else
DEPS_gnutls = nettle $(DEPS_nettle)
endif

.gnutls: gnutls
ifdef HAVE_ANDROID
	$(RECONF)
	cd $< && $(HOSTVARS) gl_cv_header_working_stdint_h=yes ./configure $(GNUTLS_CONF)
else
	cd $< && $(HOSTVARS) ./configure $(GNUTLS_CONF)
endif
	cd $</lib && $(MAKE) install
	touch $@
