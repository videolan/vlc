# mfx (Media SDK)

mfx_GITURL := https://github.com/lu-zero/mfx_dispatch.git
MFX_GITHASH := 612558419be4889ac6d059516457e83c163edcd2

ifeq ($(call need_pkg,"mfx"),)
PKGS_FOUND += mfx
endif
ifdef HAVE_WIN32
PKGS += mfx
endif

MFX_CFLAGS := $(CFLAGS)
MFX_CXXFLAGS := $(CFLAGS)

ifdef HAVE_WINSTORE
MFX_CFLAGS   += -DMEDIASDK_UWP_LOADER -DMEDIASDK_UWP_PROCTABLE
MFX_CXXFLAGS += -DMEDIASDK_UWP_LOADER -DMEDIASDK_UWP_PROCTABLE
endif

$(TARBALLS)/mfx-$(MFX_GITHASH).tar.xz:
	$(call download_git,$(mfx_GITURL),,$(MFX_GITHASH))

.sum-mfx: mfx-$(MFX_GITHASH).tar.xz
	$(call check_githash,$(MFX_GITHASH))
	touch $@

mfx: mfx-$(MFX_GITHASH).tar.xz .sum-mfx
	$(UNPACK)
	$(APPLY) $(SRC)/mfx/mfx-cpp11-fix.patch
	cd $(UNPACK_DIR) && autoreconf -ivf
	$(MOVE)

.mfx: mfx
	cd $< && $(HOSTVARS) CFLAGS="$(MFX_CFLAGS)" CXXFLAGS="$(MFX_CXXFLAGS)" ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
