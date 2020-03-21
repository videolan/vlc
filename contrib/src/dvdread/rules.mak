# DVDREAD
LIBDVDREAD_VERSION := 6.1.0
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
	$(call pkg_static,"misc/dvdread.pc.in")
	$(MOVE)

DEPS_dvdread = dvdcss $(DEPS_dvdcss)

.dvdread: dvdread
	$(REQUIRE_GPL)
	$(RECONF) -I m4
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --with-libdvdcss
	cd $< && $(MAKE) install
	touch $@
