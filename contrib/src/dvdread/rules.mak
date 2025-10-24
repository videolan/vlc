# DVDREAD
LIBDVDREAD_VERSION := 7.0.0
LIBDVDREAD_URL := https://code.videolan.org/videolan/libdvdread/-/archive/$(LIBDVDREAD_VERSION)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2
LIBDVDREAD_GITURL:=https://code.videolan.org/videolan/libdvdread.git
LIBDVDREAD_BRANCH:=master
LIBDVDREAD_GITVERSION:=fd8a73304597dc3a4fc187d0dd0cfb50af8f0a2d

ifdef BUILD_DISCS
ifdef GPL
ifndef HAVE_WINSTORE
PKGS += dvdread
endif
endif
endif
ifeq ($(call need_pkg,"dvdread >= 6.1.0"),)
PKGS_FOUND += dvdread
endif

# $(TARBALLS)/libdvdread-$(LIBDVDREAD_GITVERSION).tar.xz:
# 	$(call download_git,$(LIBDVDREAD_GITURL),$(LIBDVDREAD_BRANCH),$(LIBDVDREAD_GITVERSION))

# .sum-dvdread: libdvdread-$(LIBDVDREAD_GITVERSION).tar.xz
# 	$(call check_githash,$(LIBDVDREAD_GITVERSION))
# 	touch $@

$(TARBALLS)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2:
	$(call download,$(LIBDVDREAD_URL))

.sum-dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2

dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2 .sum-dvdread
# dvdread: libdvdread-$(LIBDVDREAD_GITVERSION).tar.xz .sum-dvdread
	$(UNPACK)
	$(MOVE)

DEPS_dvdread = dvdcss $(DEPS_dvdcss)

DVDREAD_CONF := -Dlibdvdcss=enabled

.dvdread: dvdread crossfile.meson
	$(REQUIRE_GPL)
	$(MESONCLEAN)
	$(MESON) $(DVDREAD_CONF)
	+$(MESONBUILD)
	touch $@
