# GNU Multiple Precision Arithmetic

#GMP_VERSION := 5.0.2
#GMP_URL := ftp://ftp.gmplib.org/pub/gmp-$(GMP_VERSION)/gmp-$(GMP_VERSION).tar.bz2
# last LGPLv2 version:
GMP_VERSION := 4.2.1
GMP_URL := ftp://ftp.gnu.org/pub/gnu/gmp/gmp-$(GMP_VERSION).tar.bz2

$(TARBALLS)/gmp-$(GMP_VERSION).tar.bz2:
	$(call download,$(GMP_URL))

.sum-gmp: gmp-$(GMP_VERSION).tar.bz2

gmp: gmp-$(GMP_VERSION).tar.bz2 .sum-gmp
	$(UNPACK)
	$(APPLY) $(SRC)/gmp/inline.diff
	$(APPLY) $(SRC)/gmp/ansitest.diff
	$(APPLY) $(SRC)/gmp/ansi2knr.diff
	$(MOVE)

.gmp: gmp
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
