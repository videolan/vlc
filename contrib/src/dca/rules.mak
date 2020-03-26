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
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR)
	$(call pkg_static,"./libdca/libdca.pc.in")
	$(MOVE)

.dca: libdca
	$(REQUIRE_GPL)
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -std=gnu89" ./configure $(HOSTCONF)
	cd $< && $(MAKE) -C include install
	cd $< && $(MAKE) -C libdca install
	rm -f $(PREFIX)/lib/libdts.a
	touch $@
