XAU_VERSION := 1.0.6

XAU_URL := http://xorg.freedesktop.org/releases/individual/lib/libXau-$(XAU_VERSION).tar.bz2

$(TARBALLS)/libXau-$(XAU_VERSION).tar.bz2:
	$(call download,$(XAU_URL))

ifeq ($(call need_pkg,"xau"),)
PKGS_FOUND += xau
endif

.sum-xau: libXau-$(XAU_VERSION).tar.bz2

libxau: libXau-$(XAU_VERSION).tar.bz2 .sum-xau
	$(UNPACK)
	$(MOVE)

.xau: libxau
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-xthreads
	cd $< && $(MAKE) install
