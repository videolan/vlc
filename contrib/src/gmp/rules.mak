# GNU Multiple Precision Arithmetic

GMP_VERSION := 6.1.2
GMP_URL := https://gmplib.org/download/gmp-$(GMP_VERSION)/gmp-$(GMP_VERSION).tar.bz2

GMP_CONF :=

ifdef HAVE_CLANG
ifeq ($(ARCH),mipsel)
GMP_CONF += --disable-assembly
endif
ifeq ($(ARCH),mips64el)
GMP_CONF += --disable-assembly
endif
endif
ifdef HAVE_NACL
ifeq ($(ARCH),x86_64)
GMP_CONF += --disable-assembly
endif
endif

ifdef HAVE_WIN32
ifeq ($(ARCH),arm)
GMP_CONF += --disable-assembly
endif
endif

$(TARBALLS)/gmp-$(GMP_VERSION).tar.bz2:
	$(call download_pkg,$(GMP_URL),gmp)

.sum-gmp: gmp-$(GMP_VERSION).tar.bz2

gmp: gmp-$(GMP_VERSION).tar.bz2 .sum-gmp
	$(UNPACK)
	$(APPLY) $(SRC)/gmp/ppc64.patch
	$(APPLY) $(SRC)/gmp/win-arm64.patch
	$(APPLY) $(SRC)/gmp/gmp-fix-asm-detection.patch
	# do not try the cross compiler to detect the build compiler
	cd $(UNPACK_DIR) && sed -i.orig 's/"$$CC" "$$CC $$CFLAGS $$CPPFLAGS" cc gcc c89 c99/cc gcc c89 c99/' acinclude.m4
	$(MOVE)

# GMP requires either GPLv2 or LGPLv3
.gmp: gmp
ifndef GPL
	$(REQUIRE_GNUV3)
endif
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(GMP_CONF)
	cd $< && $(MAKE) install
	touch $@
