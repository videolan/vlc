# DVDNAV

# LIBDVDNAV_VERSION := 4.2.0
# LIBDVDNAV_URL := http://dvdnav.mplayerhq.hu/releases/libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2
DVDNAV_GITURL := git://git.videolan.org/libdvdnav
LIBDVDNAV_VERSION := git

ifdef BUILD_DISCS
ifdef GPL
PKGS += dvdnav
endif
endif
ifeq ($(call need_pkg,"dvdnav"),)
PKGS_FOUND += dvdnav
endif

$(TARBALLS)/libdvdnav-git.tar.xz:
	$(call download_git,$(DVDNAV_GITURL))

# $(TARBALLS)/libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2:
# 	$(call download,$(LIBDVDNAV_URL))

.sum-dvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.xz
	$(warning $@ not implemented)
	touch $@

dvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.xz .sum-dvdnav
	$(UNPACK)
	cd $(UNPACK_DIR) && autoreconf -ivf
	$(MOVE)

DEPS_dvdnav = dvdcss dvdread

.dvdnav: dvdnav .dvdcss .dvdread
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-examples
	cd $< && $(MAKE) install
	touch $@
