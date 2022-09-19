# X protocol C Proto

XCB_PROTO_VERSION := 1.14
XCB_PROTO_URL := https://xorg.freedesktop.org/archive/individual/proto/xcb-proto-$(XCB_PROTO_VERSION).tar.gz

ifeq ($(call need_pkg,"xcb-proto"),)
PKGS_FOUND += xcb-proto
endif

$(TARBALLS)/xcb-proto-$(XCB_PROTO_VERSION).tar.gz:
	$(call download_pkg,$(XCB_PROTO_URL),xcb)

.sum-xcb-proto: xcb-proto-$(XCB_PROTO_VERSION).tar.gz

xcb-proto: xcb-proto-$(XCB_PROTO_VERSION).tar.gz .sum-xcb-proto
	$(UNPACK)
	$(MOVE)

.xcb-proto: xcb-proto
	$(RECONF)
	mkdir -p $</_build
	cd $</_build && $(HOSTVARS) ../configure $(HOSTCONF)
	$(MAKE) -C $</_build install
	touch $@
