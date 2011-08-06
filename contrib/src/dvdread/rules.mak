# DVDREAD

ifdef BUILD_DISCS
PKGS += dvdread
endif
ifeq ($(call need_pkg,"dvdread"),)
PKGS_FOUND += dvdread
endif

$(TARBALLS)/dvdread-svn.tar.xz:
	rm -Rf dvdread-svn
	$(SVN) export svn://svn.mplayerhq.hu/dvdnav/trunk/libdvdread dvdread-svn
	tar cvJ dvdread-svn > $@

.sum-dvdread: dvdread-svn.tar.xz
	$(warning Integrity check skipped.)
	touch $@

dvdread: dvdread-svn.tar.xz .sum-dvdread
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
