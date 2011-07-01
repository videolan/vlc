# liba52

A52DEC_VERSION := 0.7.4
A52DEC_URL := $(CONTRIB_VIDEOLAN)/a52dec-$(A52DEC_VERSION).tar.gz

PKGS += a52

$(TARBALLS)/a52dec-$(A52DEC_VERSION).tar.gz:
	$(call download,$(A52DEC_URL))

.sum-a52: a52dec-$(A52DEC_VERSION).tar.gz

a52dec: a52dec-$(A52DEC_VERSION).tar.gz .sum-a52
	$(UNPACK)
ifndef HAVE_FPU
	$(APPLY) $(SRC)/a52/liba52-fixed.diff
endif
	$(MOVE)

.a52: a52dec
ifdef HAVE_WIN64
	$(RECONF)
endif
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $</liba52 && $(MAKE) install
	cd $</include && $(MAKE) install
	touch $@
