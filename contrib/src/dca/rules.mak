# libdca

DCA_VERSION := 0.0.5
DCA_URL := $(VIDEOLAN)/libdca/$(DCA_VERSION)/libdca-$(DCA_VERSION).tar.bz2

ifdef HAVE_FPU
PKGS += dca
endif

$(TARBALLS)/libdca-$(DCA_VERSION).tar.bz2:
	$(call download,$(DCA_URL))

.sum-dca: libdca-$(DCA_VERSION).tar.bz2

libdca: libdca-$(DCA_VERSION).tar.bz2 .sum-dca
	$(UNPACK)
	#(cd $@-$(DCA_VERSION) && patch -p1) < $(SRC)/dca/libdca-llvm-gcc.patch
	mv $@-$(DCA_VERSION) $@
	touch $@

.dca: libdca
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
