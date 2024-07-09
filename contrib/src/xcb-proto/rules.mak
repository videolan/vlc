# X protocol C Proto

XCB_PROTO_VERSION := 1.15.2
XCB_PROTO_URL := $(XORG)/proto/xcb-proto-$(XCB_PROTO_VERSION).tar.gz

ifeq ($(call need_pkg,"xcb-proto"),)
PKGS_FOUND += xcb-proto
endif

$(TARBALLS)/xcb-proto-$(XCB_PROTO_VERSION).tar.gz:
	$(call download_pkg,$(XCB_PROTO_URL),xcb)

.sum-xcb-proto: xcb-proto-$(XCB_PROTO_VERSION).tar.gz

xcb-proto: xcb-proto-$(XCB_PROTO_VERSION).tar.gz .sum-xcb-proto
	$(UNPACK)
	$(call update_autoconfig,.)
	$(MOVE)

.xcb-proto: xcb-proto
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
