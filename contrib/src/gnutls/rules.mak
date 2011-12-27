# GnuTLS

GNUTLS_VERSION := 2.12.7
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
	$(APPLY) $(SRC)/gnutls/no-gl.patch
endif
	$(APPLY) $(SRC)/gnutls/gnutls-no-egd.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

GNUTLS_CONF := \
	--disable-gtk-doc \
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

ifeq (1,$(USE_GCRYPT))
GNUTLS_CONF += --with-libgcrypt
DEPS_gnutls = gcrypt $(DEPS_gcrypt)
else
DEPS_gnutls = nettle $(DEPS_nettle)
endif

.gnutls: gnutls
ifdef HAVE_ANDROID
	$(RECONF)
endif
	cd $< && $(HOSTVARS) ./configure $(GNUTLS_CONF)
	cd $</lib && $(MAKE) install
	touch $@
