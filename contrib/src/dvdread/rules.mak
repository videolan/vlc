# DVDREAD

LIBDVDREAD_VERSION := 4.2.0
LIBDVDREAD_URL := http://dvdnav.mplayerhq.hu/releases/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2

ifdef BUILD_DISCS
PKGS += dvdread
endif
ifeq ($(call need_pkg,"dvdread"),)
PKGS_FOUND += dvdread
endif

$(TARBALLS)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2:
	$(call download,$(LIBDVDREAD_URL))

.sum-dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2

dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2 .sum-dvdread
	$(UNPACK)
	$(APPLY) $(SRC)/dvdread/dvdread-css-static.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/dvdread/dvdread-win32.patch
endif
	$(MOVE)

DEPS_dvdread = dvdcss

.dvdread: dvdread .dvdcss
	cd $< && sh autogen.sh noconfig
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --with-libdvdcss=$(PREFIX)
	cd $< && $(MAKE) install
	touch $@
