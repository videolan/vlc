# GLIB
GLIB_VERSION := 2.75
GLIB_MINOR_VERSION := $(GLIB_VERSION).0
GLIB_URL := https://ftp.gnome.org/pub/gnome/sources/glib/$(GLIB_VERSION)/glib-$(GLIB_MINOR_VERSION).tar.xz

ifeq ($(call need_pkg,"glib-2.0 gthread-2.0"),)
PKGS_FOUND += glib
endif

DEPS_glib = ffi $(DEPS_ffi)

$(TARBALLS)/glib-$(GLIB_MINOR_VERSION).tar.xz:
	$(call download_pkg,$(GLIB_URL),glib)

.sum-glib: glib-$(GLIB_MINOR_VERSION).tar.xz

glib: glib-$(GLIB_MINOR_VERSION).tar.xz .sum-glib
	$(UNPACK)
	$(MOVE)

.glib: glib
	$(MESONCLEAN)
	$(HOSTVARS_MESON) $(MESON)
	$(MESONBUILD)
	touch $@
