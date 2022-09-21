# librist

LIBRIST_VERSION := v0.2.4
LIBRIST_URL := http://code.videolan.org/rist/librist/-/archive/$(LIBRIST_VERSION)/librist-$(LIBRIST_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += librist
endif

ifeq ($(call need_pkg,"librist >= 0.2"),)
PKGS_FOUND += librist
endif

LIBRIST_CONF = -Dbuilt_tools=false -Dtest=false

$(TARBALLS)/librist-$(LIBRIST_VERSION).tar.gz:
	$(call download_pkg,$(LIBRIST_URL),librist)

.sum-librist: librist-$(LIBRIST_VERSION).tar.gz

librist: librist-$(LIBRIST_VERSION).tar.gz .sum-librist
	$(UNPACK)
	$(APPLY) $(SRC)/librist/librist-fix-libcjson-meson.patch
	$(APPLY) $(SRC)/librist/win32-timing.patch
	$(MOVE)

.librist: librist crossfile.meson
	cd $< && rm -rf ./build
	cd $< && $(HOSTVARS_MESON) $(MESON) $(LIBRIST_CONF) build
	cd $< && cd build && ninja install
	touch $@
