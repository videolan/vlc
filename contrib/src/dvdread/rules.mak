# DVDREAD

# LIBDVDREAD_VERSION := 4.2.0
# LIBDVDREAD_URL := http://dvdnav.mplayerhq.hu/releases/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2
DVDREAD_GITURL := git://git.videolan.org/libdvdread
LIBDVDREAD_VERSION := git

ifdef BUILD_DISCS
ifdef GPL
PKGS += dvdread
endif
endif
ifeq ($(call need_pkg,"dvdread"),)
PKGS_FOUND += dvdread
endif

$(TARBALLS)/libdvdread-git.tar.xz:
	$(call download_git,$(DVDREAD_GITURL))

# $(TARBALLS)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2:
# 	$(call download,$(LIBDVDREAD_URL))

.sum-dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.xz
	$(warning $@ not implemented)
	touch $@

dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.xz .sum-dvdread
	$(UNPACK)
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/dvdread/dvdread-win32.patch
endif
	cd $(UNPACK_DIR) && autoreconf -ivf
	$(MOVE)

DEPS_dvdread = dvdcss

.dvdread: dvdread .dvdcss
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --with-libdvdcss
	cd $< && $(MAKE) install
	touch $@
