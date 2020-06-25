# libdav1d

DAV1D_VERSION := 0.7.1
DAV1D_URL := $(VIDEOLAN)/dav1d/$(DAV1D_VERSION)/dav1d-$(DAV1D_VERSION).tar.xz

PKGS += dav1d
ifeq ($(call need_pkg,"dav1d"),)
PKGS_FOUND += dav1d
endif

DAV1D_CONF = -D enable_tests=false -D enable_tools=false

$(TARBALLS)/dav1d-$(DAV1D_VERSION).tar.xz:
	$(call download_pkg,$(DAV1D_URL),dav1d)
	#~ $(call download_git,$(DAV1D_URL),,$(DAV1D_HASH))

.sum-dav1d: dav1d-$(DAV1D_VERSION).tar.xz

dav1d: dav1d-$(DAV1D_VERSION).tar.xz .sum-dav1d
	$(UNPACK)
	$(APPLY) $(SRC)/dav1d/0001-SSE2-PIC-464ca6c2.patch
	$(MOVE)

.dav1d: dav1d crossfile.meson
	cd $< && rm -rf ./build
	cd $< && $(HOSTVARS_MESON) $(MESON) $(DAV1D_CONF) build
	cd $< && cd build && ninja install
	touch $@
