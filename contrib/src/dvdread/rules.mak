# DVDREAD
LIBDVDREAD_VERSION := 7.0.1
LIBDVDREAD_URL := $(VIDEOLAN)/libdvdread/$(LIBDVDREAD_VERSION)/libdvdread-$(LIBDVDREAD_VERSION).tar.xz
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

$(TARBALLS)/libdvdread-$(LIBDVDREAD_VERSION).tar.xz:
	$(call download,$(LIBDVDREAD_URL))

.sum-dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.xz

dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.xz .sum-dvdread
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
