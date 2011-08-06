# DVDNAV

ifdef BUILD_DISCS
PKGS += dvdnav
endif
ifeq ($(call need_pkg,"dvdnav"),)
PKGS_FOUND += dvdnav
endif

$(TARBALLS)/dvdnav-svn.tar.xz:
	rm -Rf dvdnav-svn
	$(SVN) export svn://svn.mplayerhq.hu/dvdnav/trunk/libdvdnav dvdnav-svn
	tar cvJ dvdnav-svn > $@

.sum-dvdnav: dvdnav-svn.tar.xz
	$(warning Integrity check skipped.)
	touch $@

dvdnav: dvdnav-svn.tar.xz .sum-dvdnav
	$(UNPACK)
	$(APPLY) $(SRC)/dvdnav/dvdnav.patch
	$(MOVE)

DEPS_dvdnav = dvdcss dvdread

.dvdnav: dvdnav .dvdcss .dvdread
	cd $< && sh autogen.sh noconfig
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
