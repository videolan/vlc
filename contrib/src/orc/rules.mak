# orc

ORC_VERSION := 0.4.33
ORC_URL := https://gitlab.freedesktop.org/gstreamer/orc/-/archive/$(ORC_VERSION)/orc-$(ORC_VERSION).tar.bz2

ifeq ($(call need_pkg,"orc-0.4"),)
PKGS_FOUND += orc
endif

$(TARBALLS)/orc-$(ORC_VERSION).tar.bz2:
	$(call download_pkg,$(ORC_URL),orc)

.sum-orc: orc-$(ORC_VERSION).tar.bz2

orc: orc-$(ORC_VERSION).tar.bz2 .sum-orc
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(APPLY) $(SRC)/orc/0001-Fix-usage-of-pthread_jit_write_protect_np-on-macOS-a.patch

	# replace FORMAT_MESSAGE_ALLOCATE_BUFFER which may not be available in older mingw-w64 UWP
	sed -i.orig -e s/FORMAT_MESSAGE_ALLOCATE_BUFFER/0x00000100/g $(UNPACK_DIR)/orc/orccompiler.c

	$(MOVE)

ORC_CONF := -Dauto_features=disabled

.orc: orc
	$(MESONCLEAN)
	$(MESON) $(ORC_CONF)
	+$(MESONBUILD)
	touch $@
