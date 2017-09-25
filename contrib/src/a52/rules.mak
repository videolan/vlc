# liba52

A52DEC_VERSION := 0.7.4
A52DEC_URL := $(CONTRIB_VIDEOLAN)/a52/a52dec-$(A52DEC_VERSION).tar.gz

ifdef GPL
PKGS += a52
endif

$(TARBALLS)/a52dec-$(A52DEC_VERSION).tar.gz:
	$(call download,$(A52DEC_URL))

.sum-a52: a52dec-$(A52DEC_VERSION).tar.gz

a52dec: a52dec-$(A52DEC_VERSION).tar.gz .sum-a52
	$(UNPACK)
	$(APPLY) $(SRC)/a52/liba52-pic.patch
	$(APPLY) $(SRC)/a52/liba52-silence.patch
	$(APPLY) $(SRC)/a52/liba52-inline.patch
ifndef HAVE_FPU
	$(APPLY) $(SRC)/a52/liba52-fixed.diff
endif
	$(MOVE)

.a52: a52dec
	$(REQUIRE_GPL)
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $</liba52 && $(MAKE) install
	cd $</include && $(MAKE) install
	touch $@
