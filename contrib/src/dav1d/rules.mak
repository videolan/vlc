# libdav1d

DAV1D_VERSION := 1.0.0
DAV1D_URL := $(VIDEOLAN)/dav1d/$(DAV1D_VERSION)/dav1d-$(DAV1D_VERSION).tar.xz
DAV1D_HASH := 6777dd0a61ab78cc9fab92af53558ea44c135056
DAV1D_VERSION := $(DAV1D_HASH)
DAV1D_GITURL := https://code.videolan.org/videolan/dav1d.git

PKGS += dav1d
ifeq ($(call need_pkg,"dav1d"),)
PKGS_FOUND += dav1d
endif

DAV1D_CONF = -D enable_tests=false -D enable_tools=false

$(TARBALLS)/dav1d-$(DAV1D_VERSION).tar.xz:
	# $(call download_pkg,$(DAV1D_URL),dav1d)
	$(call download_git,$(DAV1D_GITURL),,$(DAV1D_HASH))

.sum-dav1d: dav1d-$(DAV1D_VERSION).tar.xz
	$(call check_githash,$(DAV1D_VERSION))
	touch $@

dav1d: dav1d-$(DAV1D_VERSION).tar.xz .sum-dav1d
	$(UNPACK)
	$(MOVE)

.dav1d: dav1d crossfile.meson
	$(MESONCLEAN)
	$(HOSTVARS_MESON) $(MESON) $(DAV1D_CONF)
	+$(MESONBUILD)
	touch $@
