# DVDNAV

LIBDVDNAV_VERSION := 6.1.0
LIBDVDNAV_URL := $(VIDEOLAN)/libdvdnav/$(LIBDVDNAV_VERSION)/libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2

ifdef BUILD_DISCS
ifdef GPL
ifndef HAVE_WINSTORE
PKGS += dvdnav
endif
endif
endif
ifeq ($(call need_pkg,"dvdnav >= 5.0.3"),)
PKGS_FOUND += dvdnav
endif

$(TARBALLS)/libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2:
	$(call download,$(LIBDVDNAV_URL))

.sum-dvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2

dvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2 .sum-dvdnav
	$(UNPACK)
	$(call pkg_static,"misc/dvdnav.pc.in")
	$(MOVE)

DEPS_dvdnav = dvdread $(DEPS_dvdread)

.dvdnav: dvdnav
	$(REQUIRE_GPL)
	$(RECONF) -I m4
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-examples
	cd $< && $(MAKE) install
	touch $@
