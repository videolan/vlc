# DVDREAD
LIBDVDREAD_VERSION := 6.0.1
LIBDVDREAD_URL := $(VIDEOLAN)/libdvdread/$(LIBDVDREAD_VERSION)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2

ifdef BUILD_DISCS
ifdef GPL
ifndef HAVE_WINSTORE
PKGS += dvdread
endif
endif
endif
ifeq ($(call need_pkg,"dvdread >= 6.0.0"),)
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
