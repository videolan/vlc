# GNU Multiple Precision Arithmetic

GMP_VERSION := 6.2.1
GMP_URL := https://gmplib.org/download/gmp/gmp-$(GMP_VERSION).tar.xz

GMP_CONF :=

ifdef HAVE_CLANG
ifeq ($(ARCH),mipsel)
GMP_CONF += --disable-assembly
endif
ifeq ($(ARCH),mips64el)
GMP_CONF += --disable-assembly
endif
endif

ifdef HAVE_WIN32
ifeq ($(ARCH),arm)
GMP_CONF += --disable-assembly
endif
endif

$(TARBALLS)/gmp-$(GMP_VERSION).tar.xz:
	$(call download_pkg,$(GMP_URL),gmp)

.sum-gmp: gmp-$(GMP_VERSION).tar.xz

gmp: gmp-$(GMP_VERSION).tar.xz .sum-gmp
	$(UNPACK)
	$(APPLY) $(SRC)/gmp/gmp-fix-asm-detection.patch
	# do not try the cross compiler to detect the build compiler
	sed -i.orig 's/"$$CC" "$$CC $$CFLAGS $$CPPFLAGS" cc gcc c89 c99/cc gcc c89 c99/' $(UNPACK_DIR)/acinclude.m4
	$(MOVE)

# GMP requires either GPLv2 or LGPLv3
.gmp: gmp
ifndef GPL
	$(REQUIRE_GNUV3)
endif
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(GMP_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
