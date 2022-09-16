# DVDREAD
LIBDVDREAD_VERSION := 6.1.3
LIBDVDREAD_URL := $(VIDEOLAN)/libdvdread/$(LIBDVDREAD_VERSION)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2

ifdef BUILD_DISCS
ifdef GPL
ifndef HAVE_WINSTORE
PKGS += dvdread
endif
endif
endif
ifeq ($(call need_pkg,"dvdread >= 6.1.0"),)
PKGS_FOUND += dvdread
endif

$(TARBALLS)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2:
	$(call download,$(LIBDVDREAD_URL))

.sum-dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2

dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2 .sum-dvdread
	$(UNPACK)
	$(APPLY) $(SRC)/dvdread/0001-ifo_types-avoid-forcing-a-higher-length-in-bitfield-.patch
	$(APPLY) $(SRC)/dvdread/0001-nav_types-make-btin_t-not-packed.patch
	$(APPLY) $(SRC)/dvdread/0002-nav_types-make-hli_t-and-pci_t-not-packed.patch
	$(call pkg_static,"misc/dvdread.pc.in")
	$(MOVE)

DEPS_dvdread = dvdcss $(DEPS_dvdcss)

DVDREAD_CONF := --with-libdvdcss

.dvdread: dvdread
	$(REQUIRE_GPL)
	$(RECONF) -I m4
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(DVDREAD_CONF)
	cd $< && $(MAKE) install
	touch $@
