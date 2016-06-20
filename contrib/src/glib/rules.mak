# GLIB
GLIB_VERSION := 2.38
GLIB_MINOR_VERSION := 2.38.2
GLIB_URL := http://ftp.gnome.org/pub/gnome/sources/glib/$(GLIB_VERSION)/glib-$(GLIB_MINOR_VERSION).tar.xz

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
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
