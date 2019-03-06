# X protocol C Proto

XCB_PROTO_VERSION := 1.12
XCB_PROTO_URL := https://www.x.org/archive/individual/xcb/xcb-proto-$(XCB_PROTO_VERSION).tar.bz2

ifeq ($(call need_pkg,"xcb-proto"),)
PKGS_FOUND += xcb-proto
endif

$(TARBALLS)/xcb-proto-$(XCB_PROTO_VERSION).tar.bz2:
	$(call download_pkg,$(XCB_PROTO_URL),xcb)

.sum-xcb-proto: xcb-proto-$(XCB_PROTO_VERSION).tar.bz2

xcb-proto: xcb-proto-$(XCB_PROTO_VERSION).tar.bz2 .sum-xcb-proto
	$(UNPACK)
	$(MOVE)

.xcb-proto: xcb-proto
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
