# mfx (Media SDK)

mfx_GITURL := git://git.videolan.org/mfx_dispatch

ifdef HAVE_WIN32
ifeq ($(call need_pkg,"mfx"),)
PKGS_FOUND += mfx
endif
endif

$(TARBALLS)/mfx-git.tar.xz:
	$(call download_git,$(mfx_GITURL))

.sum-mfx: mfx-git.tar.xz
	$(warning $@ not implemented)
	touch $@

mfx: mfx-git.tar.xz
	$(UNPACK)
	cd $(UNPACK_DIR) && autoreconf -ivf
	$(MOVE)

.mfx: mfx
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
