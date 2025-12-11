# DVDNAV

LIBDVDNAV_VERSION := 7.0.0
LIBDVDNAV_URL := $(VIDEOLAN)/libdvdnav/$(LIBDVDNAV_VERSION)/libdvdnav-$(LIBDVDNAV_VERSION).tar.xz

ifdef BUILD_DISCS
ifdef GPL
ifndef HAVE_WINSTORE
PKGS += dvdnav
endif
endif
endif
ifeq ($(call need_pkg,"dvdnav >= 5.0.3"),)
PKGS_FOUND += dvdnav
endif

$(TARBALLS)/libdvdnav-$(LIBDVDNAV_VERSION).tar.xz:
	$(call download,$(LIBDVDNAV_URL))

.sum-dvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.xz

dvdnav: libdvdnav-$(LIBDVDNAV_VERSION).tar.xz .sum-dvdnav
	$(UNPACK)
	$(APPLY) $(SRC)/dvdnav/0001-configure-don-t-use-ms-style-packing.patch
	$(MOVE)

DEPS_dvdnav = dvdread $(DEPS_dvdread)

.dvdnav: dvdnav crossfile.meson
	$(REQUIRE_GPL)
	$(MESONCLEAN)
	$(MESON)
	+$(MESONBUILD)
	touch $@
