# GCRYPT
GCRYPT_VERSION := 1.5.0
GCRYPT_URL := ftp://ftp.gnupg.org/gcrypt/libgcrypt/libgcrypt-$(GCRYPT_VERSION).tar.bz2

PKGS += gcrypt

$(TARBALLS)/libgcrypt-$(GCRYPT_VERSION).tar.bz2:
	$(call download,$(GCRYPT_URL))

.sum-gcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2

libgcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2 .sum-gcrypt
	$(UNPACK)
	$(MOVE)

DEPS_gcrypt = gpg-error

CONFIGURE_OPTS =
ifdef HAVE_WIN64
CONFIGURE_OPTS += --disable-asm
endif
ifdef HAVE_MACOSX
CONFIGURE_OPTS += --disable-aesni-support
endif
.gcrypt: libgcrypt
	#$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-ciphers=aes,des,rfc2268,arcfour --enable-digests=sha1,md5,rmd160,sha512 --enable-pubkey-ciphers=dsa,rsa,ecc $(CONFIGURE_OPTS)
	cd $< && $(MAKE) install
	touch $@
