XPROTO_VERSION := 7.0.29

XPROTO_URL := http://xorg.freedesktop.org/releases/individual/proto/xproto-$(XPROTO_VERSION).tar.bz2

$(TARBALLS)/xproto-$(XPROTO_VERSION).tar.bz2:
	$(call download_pkg,$(XPROTO_URL),xcb)

ifeq ($(call need_pkg,"xproto"),)
PKGS_FOUND += xproto
endif

.sum-xproto: xproto-$(XPROTO_VERSION).tar.bz2

xproto: xproto-$(XPROTO_VERSION).tar.bz2 .sum-xproto
	$(UNPACK)
	$(MOVE)

DEPS_xproto = xorg-macros $(DEPS_xorg-macros)

.xproto: xproto
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-xthreads
	cd $< && $(MAKE) install
	touch $@
