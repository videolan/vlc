XAU_VERSION := 1.0.9

XAU_URL := http://xorg.freedesktop.org/releases/individual/lib/libXau-$(XAU_VERSION).tar.bz2

$(TARBALLS)/libXau-$(XAU_VERSION).tar.bz2:
	$(call download_pkg,$(XAU_URL),xcb)

ifeq ($(call need_pkg,"xau"),)
PKGS_FOUND += xau
endif

.sum-xau: libXau-$(XAU_VERSION).tar.bz2

libxau: libXau-$(XAU_VERSION).tar.bz2 .sum-xau
	$(UNPACK)
	$(MOVE)

DEPS_xau = xorg-macros $(DEPS_xorg-macros) xproto $(DEPS_xproto)

XAU_CONF := --enable-xthreads

.xau: libxau
	$(RECONF)
	mkdir -p $</_build
	cd $</_build && $(HOSTVARS) ../configure $(HOSTCONF) $(XAU_CONF)
	$(MAKE) -C $</_build install
	touch $@
