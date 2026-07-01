# GCRYPT
GCRYPT_VERSION := 1.12.0
GCRYPT_URL := $(GNUGPG)/libgcrypt/libgcrypt-$(GCRYPT_VERSION).tar.bz2

PKGS += gcrypt
ifeq ($(call need_pkg,"libgcrypt"),)
PKGS_FOUND += gcrypt
endif

$(TARBALLS)/libgcrypt-$(GCRYPT_VERSION).tar.bz2:
	$(call download_pkg,$(GCRYPT_URL),gcrypt)

.sum-gcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2

gcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2 .sum-gcrypt
	$(UNPACK)
	$(call pkg_static,"src/libgcrypt.pc.in")
	$(APPLY) $(SRC)/gcrypt/disable-tests-compilation.patch
	$(APPLY) $(SRC)/gcrypt/0007-random-don-t-use-API-s-that-are-forbidden-in-UWP-app.patch
	$(APPLY) $(SRC)/gcrypt/0008-random-only-use-wincrypt-in-UWP-builds-if-WINSTORECO.patch
	$(APPLY) $(SRC)/gcrypt/0001-hwfeatures-call-SHGetFolderPathA-directly.patch

	# don't use getpid in UWP as it's not actually available
	$(APPLY) $(SRC)/gcrypt/gcrypt-uwp-getpid.patch

ifdef HAVE_ANDROID
	# disable ARM code that doesn't compile on Android
	sed -i.orig -e 's, sha1-armv7-neon.lo,,' -e 's, sha1-armv8-aarch32-ce.lo,,' $(UNPACK_DIR)/configure.ac
	sed -i.orig -e 's,#ifdef USE_NEON,#if 0 //def USE_NEON,g' $(UNPACK_DIR)/cipher/sha1.c
endif

	$(MOVE)

DEPS_gcrypt = gpg-error $(DEPS_gpg-error)

GCRYPT_CONF = \
	--enable-ciphers=aes,des,rfc2268,arcfour,chacha20 \
	--enable-digests=sha1,md5,rmd160,sha256,sha512,blake2,sha3 \
	--enable-pubkey-ciphers=dsa,rsa,ecc \
	--disable-docs

ifneq ($(call need_pkg,"gpg-error >= 1.27"),)
GCRYPT_CONF += --with-libgpg-error-prefix=$(PREFIX)
endif

ifneq ($(call need_pkg,"gpg-error >= 1.27"),)
GCRYPT_CONF += --with-libgpg-error-prefix=$(PREFIX)
endif

ifdef HAVE_WIN32
ifeq ($(ARCH),x86_64)
GCRYPT_CONF += --disable-asm --disable-padlock-support
endif
endif
ifdef HAVE_DARWIN_OS
GCRYPT_CONF += ac_cv_sys_symbol_underscore=yes
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
ifeq ($(ARCH),$(filter $(ARCH), arm aarch64))
GCRYPT_CONF += --disable-arm-crypto-support
endif
endif
ifdef HAVE_TIZEN
ifeq ($(TIZEN_ABI), x86)
GCRYPT_CONF += ac_cv_sys_symbol_underscore=no
endif
endif

.gcrypt: gcrypt
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(GCRYPT_CONF)
	+$(MAKEBUILD) bin_PROGRAMS=
	+$(MAKEBUILD) bin_PROGRAMS= install
	touch $@
