# DVDNAV

LIBDVDNAV_VERSION := 5.0.3
LIBDVDNAV_URL := $(VIDEOLAN)/libdvdnav/$(LIBDVDNAV_VERSION)/libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2

ifdef BUILD_DISCS
ifdef GPL
PKGS += dvdnav
endif
endif
ifeq ($(call need_pkg,"dvdnav > 5.0.0"),)
PKGS_FOUND += dvdnav
endif

$(TARBALLS)/libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2:
	$(call download,$(LIBDVDNAV_URL))

.sum-dvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2

dvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2 .sum-dvdnav
	$(UNPACK)
	cd $(UNPACK_DIR) && sed -i -e 's,Requires.private,Requires,g' misc/*.pc.in
	cd $(UNPACK_DIR) && autoreconf -ivf
	$(MOVE)

DEPS_dvdnav = dvdcss dvdread

.dvdnav: dvdnav .dvdcss .dvdread
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-examples
	cd $< && $(MAKE) install
	touch $@
