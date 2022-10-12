# GCRYPT
GCRYPT_VERSION := 1.10.1
GCRYPT_URL := http://www.gnupg.org/ftp/gcrypt/libgcrypt/libgcrypt-$(GCRYPT_VERSION).tar.bz2

PKGS += gcrypt
ifeq ($(call need_pkg,"libgcrypt"),)
PKGS_FOUND += gcrypt
endif

$(TARBALLS)/libgcrypt-$(GCRYPT_VERSION).tar.bz2:
	$(call download_pkg,$(GCRYPT_URL),gcrypt)

.sum-gcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2

gcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2 .sum-gcrypt
	$(UNPACK)
	$(APPLY) $(SRC)/gcrypt/disable-tests-compilation.patch
	$(APPLY) $(SRC)/gcrypt/fix-pthread-detection.patch
	$(APPLY) $(SRC)/gcrypt/0001-compat-provide-a-getpid-replacement-that-works-on-Wi.patch
	$(APPLY) $(SRC)/gcrypt/0007-random-don-t-use-API-s-that-are-forbidden-in-UWP-app.patch
	$(APPLY) $(SRC)/gcrypt/0008-random-only-use-wincrypt-in-UWP-builds-if-WINSTORECO.patch
	$(MOVE)

DEPS_gcrypt = gpg-error $(DEPS_gpg-error)

GCRYPT_CONF = \
	--enable-ciphers=aes,des,rfc2268,arcfour,chacha20 \
	--enable-digests=sha1,md5,rmd160,sha256,sha512,blake2 \
	--enable-pubkey-ciphers=dsa,rsa,ecc \
	--disable-doc

ifdef HAVE_WIN32
ifeq ($(ARCH),x86_64)
GCRYPT_CONF += --disable-asm --disable-padlock-support
endif
endif
ifdef HAVE_IOS
GCRYPT_CONF += CFLAGS="$(CFLAGS) -fheinous-gnu-extensions"
endif
ifdef HAVE_MACOSX
GCRYPT_CONF += --disable-aesni-support
ifeq ($(ARCH),aarch64)
GCRYPT_CONF += --disable-asm --disable-arm-crypto-support
endif
ifeq ($(ARCH), x86_64)
GCRYPT_CONF += ac_cv_sys_symbol_underscore=yes
endif
else
ifdef HAVE_BSD
GCRYPT_CONF += --disable-asm --disable-aesni-support
endif
endif
ifdef HAVE_ANDROID
ifeq ($(ANDROID_ABI), x86)
GCRYPT_CONF += ac_cv_sys_symbol_underscore=no
endif
ifeq ($(ANDROID_ABI), x86_64)
GCRYPT_CONF += ac_cv_sys_symbol_underscore=no
endif
ifeq ($(ARCH),aarch64)
GCRYPT_CONF += --disable-arm-crypto-support
endif
endif

.gcrypt: gcrypt
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(GCRYPT_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
