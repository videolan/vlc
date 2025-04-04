# asdcplib

ASDCPLIB_VERSION := 2.7.19

ASDCPLIB_URL := https://download.cinecert.com/asdcplib/asdcplib-$(ASDCPLIB_VERSION).tar.gz

# nettle/gmp can't be used with the LGPLv2 license
ifdef GPL
ASDCP_PKG=1
else
ifdef GNUV3
ASDCP_PKG=1
endif
endif

ifdef ASDCP_PKG
ifndef HAVE_ANDROID
ifndef HAVE_WINSTORE # FIXME uses some fordbidden SetErrorModes, GetModuleFileName in fileio.cpp
PKGS += asdcplib
endif
endif
endif

ifeq ($(call need_pkg,"asdcplib >= 1.12"),)
PKGS_FOUND += asdcplib
endif

ASDCPLIB_CXXFLAGS := $(CXXFLAGS) -std=gnu++98

$(TARBALLS)/asdcplib-$(ASDCPLIB_VERSION).tar.gz:
	$(call download_pkg,$(ASDCPLIB_URL),asdcplib)

.sum-asdcplib: asdcplib-$(ASDCPLIB_VERSION).tar.gz

asdcplib: asdcplib-$(ASDCPLIB_VERSION).tar.gz .sum-asdcplib
	$(UNPACK)
	# $(call update_autoconfig,build-aux)
	$(APPLY) $(SRC)/asdcplib/port-to-nettle.patch
	$(APPLY) $(SRC)/asdcplib/static-programs.patch
	$(APPLY) $(SRC)/asdcplib/adding-pkg-config-file.patch
	$(APPLY) $(SRC)/asdcplib/win32-cross-compilation.patch
	$(APPLY) $(SRC)/asdcplib/win32-dirent.patch
	$(APPLY) $(SRC)/asdcplib/0001-Remove-a-broken-unused-template-class.patch
	$(MOVE)

DEPS_asdcplib = nettle $(DEPS_nettle)

ASDCPLIB_CONF := --enable-freedist --enable-dev-headers --with-nettle=$(PREFIX)

ASDCPLIB_CONF += CXXFLAGS="$(ASDCPLIB_CXXFLAGS)"

.asdcplib: asdcplib
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(ASDCPLIB_CONF)
	+$(MAKEBUILD) bin_PROGRAMS=
	+$(MAKEBUILD) bin_PROGRAMS= install
	touch $@
