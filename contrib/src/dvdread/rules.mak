# DVDREAD
LIBDVDREAD_VERSION := 4.9.9
LIBDVDREAD_URL := http://download.videolan.org/pub/videolan/libdvdread/4.9.9/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2

ifdef BUILD_DISCS
ifdef GPL
PKGS += dvdread
endif
endif
ifeq ($(call need_pkg,"dvdread > 4.9.0 "),)
PKGS_FOUND += dvdread
endif

$(TARBALLS)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2:
	$(call download,$(LIBDVDREAD_URL))

.sum-dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2

dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2 .sum-dvdread
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/dvdread/dvdread-win32.patch
endif
	cd $(UNPACK_DIR) && sed -i -e 's,Requires.private,Requires,g' misc/*.pc.in
	cd $(UNPACK_DIR) && autoreconf -ivf
	$(MOVE)

DEPS_dvdread = dvdcss

.dvdread: dvdread .dvdcss
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --with-libdvdcss
	cd $< && $(MAKE) install
	touch $@
