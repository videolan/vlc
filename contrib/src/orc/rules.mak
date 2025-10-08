# orc

ORC_VERSION := 0.4.40
ORC_URL := https://gitlab.freedesktop.org/gstreamer/orc/-/archive/$(ORC_VERSION)/orc-$(ORC_VERSION).tar.bz2

ifeq ($(call need_pkg,"orc-0.4"),)
PKGS_FOUND += orc
endif

$(TARBALLS)/orc-$(ORC_VERSION).tar.bz2:
	$(call download_pkg,$(ORC_URL),orc)

.sum-orc: orc-$(ORC_VERSION).tar.bz2

orc: orc-$(ORC_VERSION).tar.bz2 .sum-orc
	$(UNPACK)
	$(MOVE)

ORC_CONF := -Dauto_features=disabled

.orc: orc crossfile.meson
	$(MESONCLEAN)
	$(MESON) $(ORC_CONF)
	+$(MESONBUILD)
	touch $@
