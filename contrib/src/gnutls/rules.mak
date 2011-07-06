# GnuTLS

GNUTLS_VERSION := 2.12.2
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
	$(APPLY) $(SRC)/gnutls/gnutls-win32.patch
	$(MOVE)

GNUTLS_CONF := \
	--disable-gtk-doc \
	--disable-cxx \
	--disable-srp-authentication \
	--disable-psk-authentication-FIXME \
	--disable-anon-authentication \
	--disable-camellia \
	--disable-extra-pki \
	--disable-openpgp-authentication \
	--disable-session-ticket \
	--disable-openssl-compatibility \
	--disable-guile \
	--with-libgcrypt \
	$(HOSTCONF)

.gnutls: gnutls .gcrypt .gpg-error
	#$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(GNUTLS_CONF)
	cd $</lib && $(MAKE) install
	touch $@
