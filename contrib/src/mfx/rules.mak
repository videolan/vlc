# mfx (Media SDK)

MFX_VERSION := 1.35.1
MFX_URL := $(GITHUB)/lu-zero/mfx_dispatch/archive/refs/tags/$(MFX_VERSION).tar.gz

ifeq ($(call need_pkg,"mfx"),)
PKGS_FOUND += mfx
endif
ifdef HAVE_WIN32
ifeq ($(filter arm aarch64, $(ARCH)),)
PKGS += mfx
endif
endif

DEPS_mfx =
ifdef HAVE_WINSTORE
DEPS_mfx += alloweduwp $(DEPS_alloweduwp)
endif

ifdef HAVE_WINSTORE
MFX_CONF := CFLAGS="$(CFLAGS) -DMEDIASDK_UWP_DISPATCHER"
MFX_CONF += CXXFLAGS="$(CXXFLAGS) -DMEDIASDK_UWP_DISPATCHER"
endif

$(TARBALLS)/mfx_dispatch-$(MFX_VERSION).tar.gz:
	$(call download_pkg,$(MFX_URL),mfx)

.sum-mfx: mfx_dispatch-$(MFX_VERSION).tar.gz

mfx: mfx_dispatch-$(MFX_VERSION).tar.gz .sum-mfx
	$(UNPACK)
	# $(call update_autoconfig,.)
	$(APPLY) $(SRC)/mfx/0001-fix-UWP-build-in-ming-w64.patch
	$(APPLY) $(SRC)/mfx/0002-fix-UWP-build-in-ming-w64.patch
	$(APPLY) $(SRC)/mfx/0001-Add-missing-mfx_dispatcher_uwp.h-.cpp.patch
	$(MOVE)

.mfx: mfx
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(MFX_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
