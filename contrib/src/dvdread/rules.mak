# DVDREAD
LIBDVDREAD_VERSION := 6.0.0
LIBDVDREAD_URL := $(VIDEOLAN)/libdvdread/$(LIBDVDREAD_VERSION)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2

ifdef BUILD_DISCS
ifdef GPL
PKGS += dvdread
endif
endif
ifeq ($(call need_pkg,"dvdread >= 5.0.3"),)
PKGS_FOUND += dvdread
endif

$(TARBALLS)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2:
	$(call download,$(LIBDVDREAD_URL))

.sum-dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2

dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2 .sum-dvdread
	$(UNPACK)
	cd $(UNPACK_DIR) && sed -i -e 's,Requires.private,Requires,g' misc/*.pc.in
	$(MOVE)

DEPS_dvdread = dvdcss

.dvdread: dvdread .dvdcss
	$(REQUIRE_GPL)
	$(RECONF) -I m4
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --with-libdvdcss
	cd $< && $(MAKE) install
	touch $@
