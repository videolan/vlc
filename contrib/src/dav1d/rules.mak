# libdav1d

DAV1D_VERSION := 0.2.2
DAV1D_URL := $(VIDEOLAN)/dav1d/$(DAV1D_VERSION)/dav1d-$(DAV1D_VERSION).tar.xz
#~ DAV1D_HASH := 1f7a7e8a6af739a05b320151d04f0f7509ae7579
#~ DAV1D_VERSION := git-$(DAV1D_HASH)
#~ DAV1D_GITURL := https://code.videolan.org/videolan/dav1d.git

PKGS += dav1d
ifeq ($(call need_pkg,"dav1d"),)
PKGS_FOUND += dav1d
endif

DAV1D_CONF = -D build_tests=false -D build_tools=false
ifdef HAVE_WIN32
DAV1D_CONF += -D win32_ver=false
endif

$(TARBALLS)/dav1d-$(DAV1D_VERSION).tar.xz:
	$(call download_pkg,$(DAV1D_URL),dav1d)
	#~ $(call download_git,$(DAV1D_URL),,$(DAV1D_HASH))

.sum-dav1d: dav1d-$(DAV1D_VERSION).tar.xz

dav1d: dav1d-$(DAV1D_VERSION).tar.xz .sum-dav1d
	$(UNPACK)
	$(MOVE)

.dav1d: dav1d crossfile.meson
	cd $< && rm -rf ./build
	cd $< && $(HOSTVARS_MESON) $(MESON) $(DAV1D_CONF) build
	cd $< && cd build && ninja install
	touch $@
