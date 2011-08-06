# LIBBLURAY

ifdef BUILD_DISCS
PKGS += libbluray
endif
ifeq ($(call need_pkg,"libbluray"),)
PKGS_FOUND += libbluray
endif

BLURAY_GITURL := git://git.videolan.org/libbluray.git

$(TARBALLS)/libbluray-git.tar.xz:
	$(call download_git,$(BLURAY_GITURL))

.sum-libbluray: libbluray-git.tar.xz
	$(warning Integrity check skipped.)
	touch $@

libbluray: libbluray-git.tar.xz .sum-libbluray
	$(UNPACK)
	$(MOVE)

.libbluray: libbluray 
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
