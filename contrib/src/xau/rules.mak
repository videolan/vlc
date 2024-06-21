XAU_VERSION := 1.0.9

XAU_URL := $(XORG)/lib/libXau-$(XAU_VERSION).tar.bz2

$(TARBALLS)/libXau-$(XAU_VERSION).tar.bz2:
	$(call download_pkg,$(XAU_URL),xcb)

ifeq ($(call need_pkg,"xau"),)
PKGS_FOUND += xau
endif

.sum-xau: libXau-$(XAU_VERSION).tar.bz2

libxau: libXau-$(XAU_VERSION).tar.bz2 .sum-xau
	$(UNPACK)
	$(call update_autoconfig,.)
	$(MOVE)

DEPS_xau = xorg-macros $(DEPS_xorg-macros) xproto $(DEPS_xproto)

XAU_CONF := --enable-xthreads

.xau: libxau
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(XAU_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
