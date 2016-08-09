# mfx (Media SDK)

mfx_GITURL := https://github.com/lu-zero/mfx_dispatch.git

ifdef HAVE_WIN32
ifeq ($(call need_pkg,"mfx"),)
PKGS_FOUND += mfx
endif
endif

$(TARBALLS)/mfx-git.tar.xz:
	$(call download_git,$(mfx_GITURL),,7adf2e4)

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
