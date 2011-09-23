# GCRYPT
GCRYPT_VERSION := 1.4.6
GCRYPT_URL := ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-$(GCRYPT_VERSION).tar.bz2

PKGS += gcrypt

$(TARBALLS)/libgcrypt-$(GCRYPT_VERSION).tar.bz2:
	$(call download,$(GCRYPT_URL))

.sum-gcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2

libgcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2 .sum-gcrypt
	$(UNPACK)
	$(APPLY) $(SRC)/gcrypt/gcrypt-nodocs.patch
	$(MOVE)

DEPS_gcrypt = gpg-error

ifdef HAVE_WIN64
ac_cv_sys_symbol_underscore=no
endif
.gcrypt: libgcrypt
	#$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-ciphers=aes,des,rfc2268,arcfour --enable-digests=sha1,md5,rmd160 --enable-pubkey-ciphers=dsa ac_cv_sys_symbol_underscore=$(ac_cv_sys_symbol_underscore)
	cd $< && $(MAKE) install
	touch $@
