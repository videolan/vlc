# GCRYPT
GCRYPT_VERSION := 1.7.10
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
	$(APPLY) $(SRC)/gcrypt/0001-random-Don-t-assume-that-_WIN64-implies-x86_64.patch
	$(APPLY) $(SRC)/gcrypt/0002-aarch64-mpi-Fix-building-the-mpi-aarch64-assembly-fo.patch
	$(APPLY) $(SRC)/gcrypt/0001-compat-provide-a-getpid-replacement-that-works-on-Wi.patch
	$(APPLY) $(SRC)/gcrypt/0007-random-don-t-use-API-s-that-are-forbidden-in-UWP-app.patch
	$(APPLY) $(SRC)/gcrypt/0008-random-only-use-wincrypt-in-UWP-builds-if-WINSTORECO.patch
ifdef HAVE_WIN64
	$(APPLY) $(SRC)/gcrypt/64bits-relocation.patch
endif
ifeq ($(CC), clang)
ifeq ($(ARCH),mips64el)
	$(APPLY) $(SRC)/gcrypt/clang-mips64.patch
endif
endif
	# Ensure we can compile the assembly code in cipher, for armv7 ios builds
	$(APPLY) $(SRC)/gcrypt/0001-cipher-Makefile.am-force-tag-CC-for-.S-files.patch
	$(MOVE)

DEPS_gcrypt = gpg-error

GCRYPT_CONF = \
	--enable-ciphers=aes,des,rfc2268,arcfour \
	--enable-digests=sha1,md5,rmd160,sha256,sha512 \
	--enable-pubkey-ciphers=dsa,rsa,ecc \
	--disable-docs

ifdef HAVE_WIN32
ifeq ($(ARCH),x86_64)
GCRYPT_CONF += --disable-asm --disable-padlock-support
endif
endif
ifdef HAVE_IOS
GCRYPT_EXTRA_CFLAGS = -fheinous-gnu-extensions
else
GCRYPT_EXTRA_CFLAGS =
endif
ifdef HAVE_MACOSX
GCRYPT_CONF += --disable-aesni-support
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
ifdef HAVE_NACL
GCRYPT_CONF += --disable-asm --disable-aesni-support ac_cv_func_syslog=no --disable-sse41-support
GCRYPT_CONF += --disable-avx-support --disable-avx2-support --disable-padlock-support
GCRYPT_CONF += --disable-amd64-as-feature-detection --disable-drng-support
GCRYPT_CONF += --disable-pclmul-support
endif

.gcrypt: gcrypt
	# Reconfiguring this requires a git repo to be available, to
	# successfully produce a nonempty mym4_revision_dec.
	cd $< && git init && git config --local user.email "cone@example.com" && git config --local user.name "Cony Cone" && \
		git commit --allow-empty -m "dummy commit"
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) $(GCRYPT_EXTRA_CFLAGS)" ./configure $(HOSTCONF) $(GCRYPT_CONF)
	cd $< && $(MAKE) install
	touch $@
