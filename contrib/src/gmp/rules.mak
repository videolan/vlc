# GNU Multiple Precision Arithmetic

GMP_VERSION := 6.1.2
GMP_URL := https://gmplib.org/download/gmp-$(GMP_VERSION)/gmp-$(GMP_VERSION).tar.bz2

GMP_CONF :=

ifeq ($(CC),clang)
ifeq ($(ARCH),mipsel)
GMP_CONF += --disable-assembly
endif
ifeq ($(ARCH),mips64el)
GMP_CONF += --disable-assembly
endif
endif

$(TARBALLS)/gmp-$(GMP_VERSION).tar.bz2:
	$(call download_pkg,$(GMP_URL),gmp)

.sum-gmp: gmp-$(GMP_VERSION).tar.bz2

gmp: gmp-$(GMP_VERSION).tar.bz2 .sum-gmp
	$(UNPACK)
	$(APPLY) $(SRC)/gmp/ppc64.patch
	$(MOVE)

# GMP requires either GPLv2 or LGPLv3
.gmp: gmp
ifndef GPL
	$(REQUIRE_GNUV3)
endif
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(GMP_CONF)
	cd $< && $(MAKE) install
	touch $@
