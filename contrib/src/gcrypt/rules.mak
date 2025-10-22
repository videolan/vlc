# GCRYPT
GCRYPT_VERSION := 1.10.1
GCRYPT_URL := $(GNUGPG)/libgcrypt/libgcrypt-$(GCRYPT_VERSION).tar.bz2

PKGS += gcrypt
ifeq ($(call need_pkg,"libgcrypt"),)
PKGS_FOUND += gcrypt
else
PKGS.tools += gcrypt
PKGS.tools.gcrypt.config-tool = libgcrypt-config
PKGS.tools.gcrypt.path = $(PREFIX)/bin/libgcrypt-config
endif

$(TARBALLS)/libgcrypt-$(GCRYPT_VERSION).tar.bz2:
	$(call download_pkg,$(GCRYPT_URL),gcrypt)

.sum-gcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2

gcrypt: libgcrypt-$(GCRYPT_VERSION).tar.bz2 .sum-gcrypt
	$(UNPACK)
	$(call pkg_static,"src/libgcrypt.pc.in")
	# $(call update_autoconfig,build-aux)
	$(APPLY) $(SRC)/gcrypt/disable-tests-compilation.patch
	$(APPLY) $(SRC)/gcrypt/fix-pthread-detection.patch
	$(APPLY) $(SRC)/gcrypt/0001-compat-provide-a-getpid-replacement-that-works-on-Wi.patch
	$(APPLY) $(SRC)/gcrypt/0007-random-don-t-use-API-s-that-are-forbidden-in-UWP-app.patch
	$(APPLY) $(SRC)/gcrypt/0008-random-only-use-wincrypt-in-UWP-builds-if-WINSTORECO.patch

	# don't use getpid in UWP as it's not actually available
	$(APPLY) $(SRC)/gcrypt/gcrypt-uwp-getpid.patch
ifdef HAVE_CROSS_COMPILE
	# disable cross-compiled command line tools that can't be run
	sed -i.orig -e 's,^bin_PROGRAMS ,bin_PROGRAMS_disabled ,g' $(UNPACK_DIR)/src/Makefile.am
endif

	$(MOVE)

DEPS_gcrypt = gpg-error $(DEPS_gpg-error)

GCRYPT_CONF = \
	--enable-ciphers=aes,des,rfc2268,arcfour,chacha20 \
	--enable-digests=sha1,md5,rmd160,sha256,sha512,blake2 \
	--enable-pubkey-ciphers=dsa,rsa,ecc \
	--disable-doc

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
