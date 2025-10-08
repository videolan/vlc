# X protocol C Proto

XCB_PROTO_VERSION := 1.17.0
XCB_PROTO_URL := $(XORG)/proto/xcb-proto-$(XCB_PROTO_VERSION).tar.xz

ifeq ($(call need_pkg,"xcb-proto"),)
PKGS_FOUND += xcb-proto
endif

$(TARBALLS)/xcb-proto-$(XCB_PROTO_VERSION).tar.xz:
	$(call download_pkg,$(XCB_PROTO_URL),xcb)

.sum-xcb-proto: xcb-proto-$(XCB_PROTO_VERSION).tar.xz

xcb-proto: xcb-proto-$(XCB_PROTO_VERSION).tar.xz .sum-xcb-proto
	$(UNPACK)
	$(call update_autoconfig,.)
	$(MOVE)

.xcb-proto: xcb-proto
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
