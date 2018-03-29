# mfx (Media SDK)

mfx_GITURL := https://github.com/lu-zero/mfx_dispatch.git
MFX_GITHASH := b3b0bc9524a8a936fb1b80ca2db45566feb2e868

ifeq ($(call need_pkg,"mfx"),)
PKGS_FOUND += mfx
endif
ifdef HAVE_WIN32
PKGS += mfx
endif

$(TARBALLS)/mfx-$(MFX_GITHASH).tar.xz:
	$(call download_git,$(mfx_GITURL),,$(MFX_GITHASH))

.sum-mfx: mfx-$(MFX_GITHASH).tar.xz
	$(call check_githash,$(MFX_GITHASH))
	touch $@

mfx: mfx-$(MFX_GITHASH).tar.xz .sum-mfx
	$(UNPACK)
	cd $(UNPACK_DIR) && autoreconf -ivf
	$(MOVE)

.mfx: mfx
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
