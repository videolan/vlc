# GNU Multiple Precision Arithmetic

GMP_VERSION := 5.0.2
GMP_URL := ftp://ftp.gmplib.org/pub/gmp-$(GMP_VERSION)/gmp-$(GMP_VERSION).tar.bz2

$(TARBALLS)/gmp-$(GMP_VERSION).tar.bz2:
	$(call download,$(GMP_URL))

.sum-gmp: gmp-$(GMP_VERSION).tar.bz2

gmp: gmp-$(GMP_VERSION).tar.bz2 .sum-gmp
	$(UNPACK)
	$(MOVE)

.gmp: gmp
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
