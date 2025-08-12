# DVDREAD
LIBDVDREAD_VERSION := 7.0.0
LIBDVDREAD_URL := $(VIDEOLAN)/libdvdread/$(LIBDVDREAD_VERSION)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2
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

$(TARBALLS)/libdvdread-$(LIBDVDREAD_GITVERSION).tar.xz:
	$(call download_git,$(LIBDVDREAD_GITURL),$(LIBDVDREAD_BRANCH),$(LIBDVDREAD_GITVERSION))

.sum-dvdread: libdvdread-$(LIBDVDREAD_GITVERSION).tar.xz
	$(call check_githash,$(LIBDVDREAD_GITVERSION))
	touch $@

# $(TARBALLS)/libdvdread-$(LIBDVDREAD_VERSION).tar.bz2:
# 	$(call download,$(LIBDVDREAD_URL))

# .sum-dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2

# dvdread: libdvdread-$(LIBDVDREAD_VERSION).tar.bz2 .sum-dvdread
dvdread: libdvdread-$(LIBDVDREAD_GITVERSION).tar.xz .sum-dvdread
	$(UNPACK)
	$(call update_autoconfig,.)
	# $(APPLY) $(SRC)/dvdread/0001-ifo_types-avoid-forcing-a-higher-length-in-bitfield-.patch
	$(call pkg_static,"misc/dvdread.pc.in")
	$(MOVE)

DEPS_dvdread = dvdcss $(DEPS_dvdcss)

DVDREAD_CONF := --with-libdvdcss

.dvdread: dvdread
	$(REQUIRE_GPL)
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(DVDREAD_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
