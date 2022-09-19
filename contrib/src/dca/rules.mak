# libdca

DCA_VERSION := 0.0.7
DCA_URL := $(VIDEOLAN)/libdca/$(DCA_VERSION)/libdca-$(DCA_VERSION).tar.bz2

ifdef HAVE_FPU
ifdef GPL
PKGS += dca
endif
endif
ifeq ($(call need_pkg,"libdca"),)
PKGS_FOUND += dca
endif

$(TARBALLS)/libdca-$(DCA_VERSION).tar.bz2:
	$(call download,$(DCA_URL))

.sum-dca: libdca-$(DCA_VERSION).tar.bz2

libdca: libdca-$(DCA_VERSION).tar.bz2 .sum-dca
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(call pkg_static,"./libdca/libdca.pc.in")
	$(MOVE)

.dca: libdca
	$(REQUIRE_GPL)
	$(RECONF)
	mkdir -p $</_build
	cd $</_build && $(HOSTVARS) ../configure $(HOSTCONF)
	$(MAKE) -C $</_build -C include install
	$(MAKE) -C $</_build -C libdca install
	rm -f $(PREFIX)/lib/libdts.a
	touch $@
