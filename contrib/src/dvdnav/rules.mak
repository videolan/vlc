# DVDNAV

# LIBDVDNAV_VERSION := 4.2.0
# LIBDVDNAV_URL := http://dvdnav.mplayerhq.hu/releases/libdvdnav-$(LIBDVDNAV_VERSION).tar.bz2
DVDNAV_GITURL := git://github.com/microe/libdvdnav
LIBDVDNAV_VERSION := git

ifdef BUILD_DISCS
PKGS += dvdnav
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
	$(APPLY) $(SRC)/dvdnav/dvdnav.patch
	$(MOVE)

DEPS_dvdnav = dvdcss dvdread

.dvdnav: dvdnav .dvdcss .dvdread
	cd $< && sh autogen.sh noconfig
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
