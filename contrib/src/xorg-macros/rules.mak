UTILS_MACROS_VERSION := 1.19.0

UTILS_MACROS_URL := https://www.x.org/archive/individual/util/util-macros-$(UTILS_MACROS_VERSION).tar.bz2

$(TARBALLS)/util-macros-$(UTILS_MACROS_VERSION).tar.bz2:
	$(call download_pkg,$(UTILS_MACROS_URL),xcb)

ifeq ($(call need_pkg,"xorg-macros"),)
PKGS_FOUND += xorg-macros
endif

.sum-xorg-macros: util-macros-$(UTILS_MACROS_VERSION).tar.bz2

xorg-macros: util-macros-$(UTILS_MACROS_VERSION).tar.bz2 .sum-xorg-macros
	$(UNPACK)
	$(MOVE)

.xorg-macros: xorg-macros
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
