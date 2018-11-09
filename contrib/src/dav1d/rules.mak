# libdav1d

DAV1D_HASH := 8c95771dfa7a0bdb542eef8924bd0d3009e5efff
DAV1D_VERSION := git-$(DAV1D_HASH)
DAV1D_GITURL := https://code.videolan.org/videolan/dav1d.git

PKGS += dav1d
ifeq ($(call need_pkg,"dav1d"),)
PKGS_FOUND += dav1d
endif

DAV1D_CONF = -D build_tests=false -D build_tools=false
ifdef HAVE_WIN32
DAV1D_CONF += -D win32_ver=false
endif

$(TARBALLS)/dav1d-$(DAV1D_VERSION).tar.xz:
	$(call download_git,$(DAV1D_GITURL),,$(DAV1D_HASH))

.sum-dav1d: dav1d-$(DAV1D_VERSION).tar.xz
	$(call check_githash,$(DAV1D_HASH))
	touch $@

dav1d: dav1d-$(DAV1D_VERSION).tar.xz .sum-dav1d
	$(UNPACK)
	$(APPLY) $(SRC)/dav1d/0001-add-an-option-to-disable-the-.rc-compilation.patch
	$(MOVE)

.dav1d: dav1d crossfile.meson
	cd $< && rm -rf ./build
	cd $< && $(HOSTVARS_MESON) $(MESON) $(DAV1D_CONF) build
	cd $< && cd build && ninja install
	touch $@
