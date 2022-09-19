# asdcplib

ASDCPLIB_VERSION := 2.7.19

ASDCPLIB_URL := http://download.cinecert.com/asdcplib/asdcplib-$(ASDCPLIB_VERSION).tar.gz

ifndef HAVE_IOS
ifndef HAVE_ANDROID
ifndef HAVE_WINSTORE
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
	$(APPLY) $(SRC)/asdcplib/port-to-nettle.patch
	$(APPLY) $(SRC)/asdcplib/static-programs.patch
	$(APPLY) $(SRC)/asdcplib/adding-pkg-config-file.patch
	$(APPLY) $(SRC)/asdcplib/win32-cross-compilation.patch
	$(APPLY) $(SRC)/asdcplib/win32-dirent.patch
	$(MOVE)

DEPS_asdcplib = nettle $(DEPS_nettle)

ASDCPLIB_CONF := --enable-freedist --enable-dev-headers --with-nettle=$(PREFIX)

ASDCPLIB_CONF += CXXFLAGS="$(ASDCPLIB_CXXFLAGS)"

.asdcplib: asdcplib
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(ASDCPLIB_CONF)
	cd $< && $(MAKE) install
	touch $@
